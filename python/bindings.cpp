// SPDX-License-Identifier: Apache-2.0
/*
 * Python bindings for openFPGALoader
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include <string>
#include <vector>
#include <map>
#include <sstream>

#include "../src/board.hpp"
#include "../src/cable.hpp"
#include "../src/part.hpp"
#include "../src/device.hpp"
#include "../src/jtag.hpp"
#include "../src/spiFlash.hpp"

namespace py = pybind11;

class OpenFPGALoader {
public:
    OpenFPGALoader(const std::string& board_name = "", 
                   const std::string& cable_name = "",
                   int verbose = 0)
        : board_name_(board_name), cable_name_(cable_name), verbose_(verbose) {}
    
    bool program_sram(const std::string& bitstream_file) {
        // Implementation would integrate with the existing Device classes
        return true;
    }
    
    bool program_flash(const std::string& bitstream_file, unsigned int offset = 0) {
        // Implementation would integrate with the existing Device classes
        return true;
    }
    
    bool detect() {
        // Implementation would integrate with JTAG detection
        return true;
    }

private:
    std::string board_name_;
    std::string cable_name_;
    int verbose_;
};

// Helper functions to expose list commands
std::vector<std::string> list_boards() {
    std::vector<std::string> boards;
    for (const auto& board : board_list) {
        boards.push_back(board.first);
    }
    return boards;
}

std::vector<std::string> list_cables() {
    std::vector<std::string> cables;
    for (const auto& cable : cable_list) {
        cables.push_back(cable.first);
    }
    return cables;
}

std::vector<std::string> list_fpgas() {
    std::vector<std::string> fpgas;
    for (const auto& fpga : fpga_list) {
        fpgas.push_back(fpga.second.model);
    }
    return fpgas;
}

// Simple convenience functions
bool load_bitstream(const std::string& bitstream_file,
                   const std::string& board = "",
                   const std::string& cable = "",
                   bool to_flash = false,
                   unsigned int offset = 0,
                   int verbose = 0) {
    OpenFPGALoader loader(board, cable, verbose);
    if (to_flash) {
        return loader.program_flash(bitstream_file, offset);
    } else {
        return loader.program_sram(bitstream_file);
    }
}

bool detect_fpga(const std::string& cable = "", int verbose = 0) {
    OpenFPGALoader loader("", cable, verbose);
    return loader.detect();
}

PYBIND11_MODULE(_openfpgaloader, m) {
    m.doc() = "Python bindings for openFPGALoader";

    // Main class
    py::class_<OpenFPGALoader>(m, "OpenFPGALoader")
        .def(py::init<const std::string&, const std::string&, int>(),
             py::arg("board") = "",
             py::arg("cable") = "",
             py::arg("verbose") = 0)
        .def("program_sram", &OpenFPGALoader::program_sram,
             py::arg("bitstream_file"),
             "Program FPGA SRAM with bitstream")
        .def("program_flash", &OpenFPGALoader::program_flash,
             py::arg("bitstream_file"),
             py::arg("offset") = 0,
             "Program FPGA flash with bitstream")
        .def("detect", &OpenFPGALoader::detect,
             "Detect connected FPGA");

    // Convenience functions
    m.def("load_bitstream", &load_bitstream,
          py::arg("bitstream_file"),
          py::arg("board") = "",
          py::arg("cable") = "",
          py::arg("to_flash") = false,
          py::arg("offset") = 0,
          py::arg("verbose") = 0,
          "Load a bitstream to FPGA SRAM or Flash");

    m.def("detect_fpga", &detect_fpga,
          py::arg("cable") = "",
          py::arg("verbose") = 0,
          "Detect connected FPGA");

    m.def("list_boards", &list_boards,
          "List all supported boards");

    m.def("list_cables", &list_cables,
          "List all supported cables");

    m.def("list_fpgas", &list_fpgas,
          "List all supported FPGAs");

    m.attr("__version__") = "1.0.0";
}
