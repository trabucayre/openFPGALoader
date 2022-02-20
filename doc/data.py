from typing import List, Union
from pathlib import Path
from dataclasses import dataclass
from yaml import load as yaml_load, Loader as yaml_loader, dump as yaml_dump
from tabulate import tabulate


ROOT = Path(__file__).resolve().parent


@dataclass
class Board:
    ID: str
    Description: str = None
    URL: str = None
    FPGA: str = None
    Memory: str = None
    Flash: str = None
    Constraints: str = None


def ReadBoardDataFromYAML():
    with (ROOT / 'boards.yml').open('r', encoding='utf-8') as fptr:
        data = [Board(**item) for item in yaml_load(fptr, yaml_loader)]
    return data


def BoardDataToTable(data, tablefmt: str = "rst"):
    def processConstraints(constraints):
        if constraints is None:
            return None
        if isinstance(constraints, str):
            constraints = [constraints]
        return " ".join([f":ref:`{item} ➚ <constraints:boards:{item.lower()}>`" for item in constraints])

    return tabulate(
        [
            [
                item.ID,
                f"`{item.Description} <{item.URL}>`__",
                item.FPGA,
                item.Memory,
                item.Flash,
                processConstraints(item.Constraints)
            ] for item in data
        ],
        headers=["Board name", "Description", "FPGA", "Memory", "Flash", "Constraints"],
        tablefmt=tablefmt
    )


@dataclass
class FPGA:
    Model: Union[str, List[str]]
    Description: str
    URL: str = None
    Memory: str = None
    Flash: str = None


def ReadFPGADataFromYAML():
    with (ROOT / 'FPGAs.yml').open('r', encoding='utf-8') as fptr:
        data = yaml_load(fptr, yaml_loader)
        for vendor, content in data.items():
            data[vendor] = [FPGA(**item) for item in content]
    return data


def FPGADataToTable(data, tablefmt: str = "rst"):
    return tabulate(
        [
            [
                f":ref:`{vendor} <{vendor.lower().replace(' ','')}>`",
                f"`{item.Description} <{item.URL}>`__",
                item.Model if isinstance(item.Model, str) else ', '.join(item.Model),
                item.Memory,
                item.Flash
            ] for vendor, content in data.items() for item in content
        ],
        headers=["Vendor", "Description", "Model", "Memory", "Flash"],
        tablefmt=tablefmt
    )


@dataclass
class Cable:
    Name: str
    Description: str
    URL: str = None
    Note: str = None


def ReadCableDataFromYAML():
    with (ROOT / 'cable.yml').open('r', encoding='utf-8') as fptr:
        data = yaml_load(fptr, yaml_loader)
        for keyword, content in data.items():
            data[keyword] = [Cable(**item) for item in content]
    return data


def CableDataToTable(data, tablefmt: str = "rst"):
    def processURL(name, url):
        if url is None:
            return f"{name}"
        else:
            return f"`{name} <{url}>`__"
    return tabulate(
        [
            [
                f"{vendor}",
                processURL(item.Name, item.URL),
                item.Description
            ] for vendor, content in data.items() for item in content
        ],
        headers=["keyword", "Name", "Description"],
        tablefmt=tablefmt
    )
