from setuptools import setup, find_packages

setup(
    name='pi-comp',
    version='1.0.0',
    packages=find_packages(where="src"),
    package_dir={"": "src"},
    install_requires=["pyserial>=3.5"],
    extras_require={
        "dev": ["pytest>=7.0"],
    },
    entry_points={
        'console_scripts': [
            'pi-comp = pi_comp.main:run',
        ]
    }
)