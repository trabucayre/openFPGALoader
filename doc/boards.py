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


def ReadDataFromYAML():
    with (ROOT / 'boards.yml').open('r', encoding='utf-8') as fptr:
        data = [Board(**item) for item in yaml_load(fptr, yaml_loader)]
    return data


def DataToTable(data, tablefmt: str = "rst"):
    return tabulate(
        [
            [
                item.ID,
                f"`{item.Description} <{item.URL}>`__",
                item.FPGA,
                item.Memory,
                item.Flash
            ] for item in data
        ],
        headers=["Board name", "Description", "FPGA", "Memory", "Flash"],
        tablefmt=tablefmt
    )
