.PHONY: run install uninstall

run: install
	pi-comp

install:
	pipx uninstall pi-comp 2>/dev/null || true
	pipx install -e .

uninstall:
	pipx uninstall pi-comp 2>/dev/null || true