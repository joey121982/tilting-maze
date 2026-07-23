from setuptools import setup, find_packages

setup(
    name='pi-comp',
    version='1.0.0',
    packages=find_packages(where="src"),
    package_dir={"": "src"},
    install_requires=[],
    entry_points={
        'console_scripts': [
            'pi-comp = main:run',
        ]
    }
)