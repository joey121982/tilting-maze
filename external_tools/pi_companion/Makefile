.PHONY: run install uninstall oracle sim test clean venv

VENV := .venv
PY   := $(VENV)/bin/python
PIP  := $(VENV)/bin/pip

run: install
	pi-comp

install:
	pipx uninstall pi-comp 2>/dev/null || true
	pipx install -e .

uninstall:
	pipx uninstall pi-comp 2>/dev/null || true

oracle:
	$(MAKE) -C tools

$(VENV)/bin/python:
	python3 -m venv $(VENV)
	$(PIP) install -q -e '.[dev]'

venv: $(VENV)/bin/python

sim: venv oracle
	PYTHONPATH=src $(PY) -m pi_comp.main --simulate

test: venv oracle
	$(PY) -m pytest tests -q

clean:
	$(MAKE) -C tools clean
	rm -rf .pytest_cache
	rm -rf $(VENV)
	find src tests -name __pycache__ -type d -exec rm -rf {} +
