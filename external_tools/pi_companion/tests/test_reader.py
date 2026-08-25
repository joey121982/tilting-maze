"""Tests for line framing.

Framing is the part most likely to break and least likely to be noticed: it only
misbehaves when a message straddles a read boundary, which on a real link depends on
timing. These tests force those boundaries deliberately.
"""

import queue
import threading

from pi_comp import reader


class FakeSerial:
    """Minimal stand-in for serial.Serial, feeding fixed chunks to the framing loop.

    Only the two members _pump() touches are implemented. When the chunks run out it
    trips the reader's stop event so the loop terminates instead of spinning.
    """

    def __init__(self, chunks, done: threading.Event) -> None:
        self._chunks = list(chunks)
        self._done = done
        self.in_waiting = 0

    def read(self, _n):
        if self._chunks:
            return self._chunks.pop(0)
        self._done.set()
        return b""


def pump(chunks):
    """Run the framing loop over fixed chunks, returning the events it produced."""
    r = reader.SerialReader("fake", events=queue.Queue())
    r._pump(FakeSerial(chunks, r._stop))

    events = []
    while True:
        try:
            events.append(r.events.get_nowait())
        except queue.Empty:
            return events


def messages(events):
    return [e["msg"] for e in events if e["kind"] == "message"]


def bad_lines(events):
    return [e for e in events if e["kind"] == "bad_line"]


# -- framing ---------------------------------------------------------------------


def test_single_message_in_one_chunk():
    events = pump([b'{"type":"status","tick":1}\n'])
    assert [m["tick"] for m in messages(events)] == [1]
    assert not bad_lines(events)


def test_several_messages_in_one_chunk():
    events = pump([b'{"type":"status","tick":1}\n{"type":"status","tick":2}\n'])
    assert [m["tick"] for m in messages(events)] == [1, 2]


def test_message_split_across_chunks_is_reassembled():
    """readline() would have lost this one at a timeout; buffering must not."""
    events = pump([b'{"type":"sta', b'tus","tick', b'":7}\n'])
    assert [m["tick"] for m in messages(events)] == [7]
    assert not bad_lines(events)


def test_message_split_one_byte_at_a_time():
    raw = b'{"type":"status","tick":99}\n'
    events = pump([raw[i : i + 1] for i in range(len(raw))])
    assert [m["tick"] for m in messages(events)] == [99]


def test_connecting_mid_message_drops_only_the_partial_line():
    """The case the newline framing exists for.

    A companion that opens the port halfway through a message sees a meaningless tail
    first. That one line must be dropped and everything after it must be fine.
    """
    events = pump([b'us","tick":4}\n{"type":"status","tick":5}\n{"type":"status","tick":6}\n'])

    assert [m["tick"] for m in messages(events)] == [5, 6]
    assert len(bad_lines(events)) == 1


def test_trailing_partial_message_is_not_emitted():
    """A message with no terminating newline yet is held, not guessed at."""
    events = pump([b'{"type":"status","tick":1}\n{"type":"sta'])
    assert [m["tick"] for m in messages(events)] == [1]
    assert not bad_lines(events)


def test_blank_lines_are_ignored():
    events = pump([b'\n\n{"type":"status","tick":1}\n\n'])
    assert len(messages(events)) == 1
    assert not bad_lines(events)


def test_crlf_line_endings_are_tolerated():
    events = pump([b'{"type":"status","tick":1}\r\n'])
    assert [m["tick"] for m in messages(events)] == [1]
    assert not bad_lines(events)


# -- robustness ------------------------------------------------------------------


def test_invalid_utf8_does_not_kill_the_thread():
    """Line noise on a TTL link is normal; it must cost one line, not the connection."""
    events = pump([b'\xff\xfe\x00bad\n{"type":"status","tick":3}\n'])

    assert [m["tick"] for m in messages(events)] == [3]
    assert len(bad_lines(events)) == 1


def test_garbage_with_no_newline_is_dropped_and_resyncs():
    """A wrong baud rate looks like this: bytes forever with no frame delimiter."""
    flood = b"\xa5" * (reader.MAX_LINE_BYTES + 100)
    events = pump([flood, b'{"type":"status","tick":8}\n'])

    dropped = bad_lines(events)
    assert any("no frame delimiter" in e["error"] for e in dropped)
    assert [m["tick"] for m in messages(events)] == [8]


def test_structurally_invalid_maze_is_reported_not_drawn():
    events = pump([b'{"type":"maze","walls":[[0,1]],"path":[]}\n'])

    assert not messages(events)
    assert len(bad_lines(events)) == 1
