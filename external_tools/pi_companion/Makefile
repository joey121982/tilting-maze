.PHONY: run install uninstall oracle sim test clean

run: install
	pi-comp

install:
	pipx uninstall pi-comp 2>/dev/null || true
	pipx install -e .

uninstall:
	pipx uninstall pi-comp 2>/dev/null || true

oracle:
	$(MAKE) -C tools

sim: oracle
	PYTHONPATH=src python3 -m pi_comp.main --simulate

test: oracle
	python3 -m pytest tests -q

clean:
	$(MAKE) -C tools clean
	rm -rf .pytest_cache
	find src tests -name __pycache__ -type d -exec rm -rf {} +
