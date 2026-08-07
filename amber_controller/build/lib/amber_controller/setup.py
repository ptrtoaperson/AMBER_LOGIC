from setuptools import setup

setup(
    name="amber_controller",
    version="0.1.0",
    description="SemiQon STM32 voltage source command CLI and module",
    packages=["amber_controller"],
    package_dir={"amber_controller": "."},
    entry_points={
        "console_scripts": [
            "amber_controller=amber_controller.cli:main",
        ]
    },
)
