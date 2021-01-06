/*
 * Copyright (C) 2009-2012 Chris McClelland
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file libfpgalink.h
 *
 * The <b>FPGALink</b> library makes it easier to talk to an FPGA over USB (via a suitable micro).
 *
 * It performs three classes of function:
 * - Load device firmware and EEPROM (specific to Cypress FX2LP).
 * - Program an FPGA or CPLD using JTAG or one of the proprietary serial or parallel algorithms.
 * - Read and write (over USB) up to 128 byte-wide data channels in the target FPGA.
 * - Manipulate microcontroller digital I/O and SPI port(s).
 */
#ifndef FPGALINK_H
#define FPGALINK_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

	// ---------------------------------------------------------------------------------------------
	// Type declarations
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name Types
	 * @{
	 */
	/**
	 * Return codes from the functions.
	 */
	typedef enum {
		FL_SUCCESS = 0,          ///< The operation completed successfully.
		FL_ALLOC_ERR,            ///< There was a memory allocation error.
		FL_USB_ERR,              ///< There was some USB-related problem.
		FL_PROTOCOL_ERR,         ///< The device is probably not a valid FPGALink device.
		FL_EARLY_TERM,           ///< The device did an unexpected early read termination.
		FL_FX2_ERR,              ///< There was some problem talking to the FX2 chip.
		FL_FILE_ERR,             ///< There was a file-related problem.
		FL_UNSUPPORTED_CMD_ERR,  ///< The XSVF file contains an unsupported command.
		FL_UNSUPPORTED_DATA_ERR, ///< The XSVF file contains an unsupported XENDIR or XENDDR.
		FL_UNSUPPORTED_SIZE_ERR, ///< The XSVF file requires more buffer space than is available.
		FL_SVF_PARSE_ERR,        ///< The SVF file is not parseable.
		FL_CONF_FORMAT,          ///< The supplied programming config is malformed.
		FL_PROG_PORT_MAP,        ///< There was a problem remapping ports for programming.
		FL_PROG_SEND,            ///< There was a problem sending data during programming.
		FL_PROG_RECV,            ///< There was a problem receiving data during programming.
		FL_PROG_SHIFT,           ///< There was a problem with the requested shift operation.
		FL_PROG_JTAG_FSM,        ///< There was a problem navigating the JTAG state machine.
		FL_PROG_JTAG_CLOCKS,     ///< There was a problem issuing clocks during programming.
		FL_PROG_SVF_COMPARE,     ///< An SVF compare operation failed.
		FL_PROG_SVF_UNKNOWN_CMD, ///< An unknown SVF command was encountered.
		FL_PROG_ERR,             ///< The device failed to start after programming.
		FL_PORT_IO,              ///< There was a problem doing port I/O.
		FL_BAD_STATE,            ///< You're trying to do something that is illegal in this state.
		FL_INTERNAL_ERR          ///< An internal error occurred. Please report it!
	} FLStatus;

	/**
	 * Enum used by \c progGetPort() and \c progGetBit() to identify the programming pins.
	 */
	typedef enum {
		LP_CHOOSE, ///< These aren't the droids you're looking for. Move along.
		LP_MISO,   ///< The master-in, slave-out pin (TDO).
		LP_MOSI,   ///< The master-out, slave-in pin (TDI).
		LP_SS,     ///< The slave-select pin (TMS).
		LP_SCK,    ///< The serial clock pin (TCK).
		LP_D8      ///< The parallel data port.
	} LogicalPort;

	/**
	 * Enum used by \c flSingleBitPortAccess() to configure the pin direction and drive.
	 */
	typedef enum {
		PIN_UNUSED, ///< These are also not the droids you're looking for. Keep moving along.
		PIN_HIGH,   ///< Configure the pin as an output and drive it high.
		PIN_LOW,    ///< Configure the pin as an output and drive it low.
		PIN_INPUT   ///< Configure the pin as an input.
	} PinConfig;

	/**
	 * Enum used by \c spiSend() and \c spiRecv() to set the order bits are clocked in.
	 */
	typedef enum {
		SPI_MSBFIRST,  ///< Clock each byte most-significant bit first.
		SPI_LSBFIRST   ///< Clock each byte least-significant bit first.
	} BitOrder;
	//@}

	// Forward declarations
	struct FLContext; // Opaque FPGALink context
	struct Buffer;    // Dynamic binary buffer (see libbuffer)

	// ---------------------------------------------------------------------------------------------
	// Miscellaneous functions
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name Miscellaneous Functions
	 * @{
	 */
	/**
	 * @brief Initialise the library with the given log level.
	 *
	 * This may fail if LibUSB cannot talk to the USB host controllers through its kernel driver
	 * (e.g a Linux kernel with USB support disabled, or a machine lacking a USB host controller).
	 *
	 * @param debugLevel 0->none, 1, 2, 3->lots.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_USB_ERR if there were problems initialising LibUSB.
	 */
	DLLEXPORT(FLStatus) flInitialise(int debugLevel, const char **error);

	/**
	 * @brief Shutdown the library.
	 *
	 * This frees up any resources used by the libray itself (e.g LibUSB context).
	 */
	DLLEXPORT(void) flShutdown();

	/**
	 * @brief Free an error allocated when one of the other functions fails.
	 *
	 * @param err An error message previously allocated by one of the other library functions.
	 */
	DLLEXPORT(void) flFreeError(const char *err);
	//@}

	// ---------------------------------------------------------------------------------------------
	// Connection lifecycle functions
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name Connection Lifecycle
	 * @{
	 */
	/**
	 * @brief Open a connection to the FPGALink device at the specified VID and PID.
	 *
	 * Connects to the device and verifies it's an FPGALink device, then queries its capabilities.
	 *
	 * @param vp The Vendor/Product (i.e VVVV:PPPP) of the FPGALink device. You may also specify
	 *            an optional device ID (e.g 1D50:602B:0004). If no device ID is supplied, it
	 *            selects the first device with matching VID:PID.
	 * @param handle A pointer to a <code>struct FLContext*</code> which will be set on exit to
	 *            point at a newly-allocated context structure. Responsibility for this allocated
	 *            memory (and its associated USB resources) passes to the caller and must be freed
	 *            with \c flClose(). Will be set \c NULL if an error occurs.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if all is well (\c *handle is valid).
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 *     - \c FL_USB_ERR if the VID:PID is invalid or the device cannot be found or opened.
	 *     - \c FL_PROTOCOL_ERR if the device is not an FPGALink device.
	 */
	DLLEXPORT(FLStatus) flOpen(
		const char *vp, struct FLContext **handle, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Close an existing connection to an FPGALink device.
	 *
	 * If the handle is \c NULL, this function does nothing.
	 *
	 * @param handle The handle returned by \c flOpen(), or \c NULL.
	 */
	DLLEXPORT(void) flClose(
		struct FLContext *handle
	);
	//@}

	// ---------------------------------------------------------------------------------------------
	// Get device capabilities and status
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name Device Capabilities and Status
	 * @{
	 */
	/**
	 * @brief Check if the given device is actually connected to the system.
	 *
	 * The LibUSB devices in the system are searched for a device with the given VID:PID.
	 *
	 * There is a short period of time following a call to \c flLoadStandardFirmware() or
	 * \c flLoadCustomFirmware() during which this function will still return with
	 * <code>*isAvailable == 1</code> for the "current" VID:PID, so when you load new firmware, it's
	 * important to either wait a while before calling this function, or alternatively just ensure
	 * that the "new" VID:PID is different from the "current" VID:PID to avoid such false positives.
	 *
	 * @param vp The Vendor/Product (i.e VVVV:PPPP) of the FPGALink device. You may also specify
	 *            an optional device ID (e.g 1D50:602B:0004). If no device ID is supplied, it
	 *            selects the first device with matching VID:PID.
	 * @param isAvailable A pointer to an 8-bit integer which will be set on exit to 1 if available
	 *            else 0.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if all is well (\c *isAvailable is valid).
	 *     - \c FL_USB_ERR if the VID:PID is invalid or if no USB buses were found (did you
	 *            remember to call \c flInitialise()?).
	 */
	DLLEXPORT(FLStatus) flIsDeviceAvailable(
		const char *vp, uint8 *isAvailable, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Check to see if the device supports NeroProg.
	 *
	 * NeroProg is the collective name for all the various programming algorithms supported by
	 * FPGALink, including but not limited to JTAG. An affirmative response means you are free to
	 * call \c flProgram(), \c flProgramBlob(), \c jtagScanChain(), \c progOpen(), \c progClose(),
	 * \c jtagShiftInOnly(), \c jtagShiftInOut(), \c jtagClockFSM(), \c jtagClocks(),
	 * \c progGetPort(), \c progGetBit(), \c spiSend(), \c spiRecv() and \c spiBitSwap().
	 *
	 * This function merely returns a flag determined by \c flOpen(), so it cannot fail.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @returns An 8-bit integer: 1 if the device supports NeroProg, else 0.
	 */
	DLLEXPORT(uint8) flIsNeroCapable(struct FLContext *handle);

	/**
	 * @brief Check to see if the device supports CommFPGA.
	 *
	 * CommFPGA is a set of channel read/write protocols. The micro may implement several
	 * different CommFPGA protocols, distinguished by the chosen conduit. A micro will typically
	 * implement its first CommFPGA protocol on conduit 1, and additional protocols on conduit
	 * 2, 3 etc. Conduit 0 is reserved for communication over JTAG using a virtual TAP
	 * state machine implemented in the FPGA, and is not implemented yet.
	 *
	 * This function returns 1 if the micro supports CommFPGA on the chosen conduit, else 0.
	 *
	 * Note that this function can only know the capabilities of the micro itself; it cannot
	 * determine whether the FPGA contains suitable logic to implement the protocol, or even
	 * whether there is an FPGA physically wired to the micro in the first place.
	 *
	 * An affirmative response means you are free to call \c flIsFPGARunning(),
	 * \c flReadChannel(), \c flWriteChannel(), \c flSetAsyncWriteChunkSize(),
	 * \c flWriteChannelAsync(), \c flFlushAsyncWrites() \c flAwaitAsyncWrites(),
	 * \c flReadChannelAsyncSubmit(), and \c flReadChannelAsyncAwait().
	 *
	 * This function merely returns information determined by \c flOpen(), so it cannot fail.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param conduit The conduit you're interested in (this will typically be 1).
	 * @returns An 8-bit integer: 1 if the device supports CommFPGA, else 0.
	 */
	DLLEXPORT(uint8) flIsCommCapable(struct FLContext *handle, uint8 conduit);

	/**
	 * @brief Get the firmware ID.
	 *
	 * Each firmware (or fork of an existing firmware) has its own 16-bit ID, which this function
	 * retrieves.
	 *
	 * This function merely returns information determined by \c flOpen(), so it cannot fail.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @returns A 16-bit unsigned integer giving the firmware ID.
	 */
	DLLEXPORT(uint16) flGetFirmwareID(struct FLContext *handle);

	/**
	 * @brief Get the firmware version.
	 *
	 * Each firmware knows the GitHub tag from which is was built, or if it was built from a trunk,
	 * it knows the date on which it was built. This function returns a 32-bit integer giving that
	 * information. If printed as a hex number, it gives an eight-digit ISO date.
	 *
	 * This function merely returns information determined by \c flOpen(), so it cannot fail.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @returns A 32-bit unsigned integer giving the firmware version.
	 */
	DLLEXPORT(uint32) flGetFirmwareVersion(struct FLContext *handle);
	//@}

	// ---------------------------------------------------------------------------------------------
	// CommFPGA channel read/write functions (only if flIsCommCapable() returns true)
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name CommFPGA Operations
	 * @{
	 */

	/**
	 * @brief Select a different conduit.
	 *
	 * Select a different conduit for CommFPGA communication. Typically a micro will implement its
	 * first CommFPGA protocol on conduit 1. It may or may not also implement others on conduit 2,
	 * 3, 4 etc. It may also implement comms-over-JTAG using a virtual TAP FSM on the FPGA. You can
	 * use \c flIsCommCapable() to determine whether the micro supports CommFPGA on a given conduit.
	 *
	 * If mixing NeroProg operations with CommFPGA operations, it \b may be necessary to switch
	 * conduits. For example, if your PCB is wired to use some of the CommFPGA signals during
	 * programming, you will have to switch back and forth. But if the pins used for CommFPGA are
	 * independent of the pins used for NeroProg, you need only select the correct conduit on
	 * startup and then leave it alone.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param conduit The conduit to select (current range 0-15).
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_USB_ERR if the device doesn't respond, or the conduit is out of range.
	 */
	DLLEXPORT(FLStatus) flSelectConduit(
		struct FLContext *handle, uint8 conduit, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Check to see if the FPGA is running.
	 *
	 * This may only be called if \c flIsCommCapable() returns true. It merely verifies that
	 * the FPGA is asserting that it's ready to read commands on the chosen conduit. Some conduits
	 * may not have the capability to determine this, and will therefore just optimistically report
	 * true. Before calling this function you should verify that the FPGALink device actually
	 * supports CommFPGA using \c flIsCommCapable(), and select the conduit you wish to
	 * use with \c flSelectConduit().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param isRunning A pointer to an 8-bit integer which will be set on exit to 1 if the FPGA
	 *            is running, else 0.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if all is well (<code>*isRunning</code> is valid).
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 */
	DLLEXPORT(FLStatus) flIsFPGARunning(
		struct FLContext *handle, uint8 *isRunning, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Synchronously read one or more bytes from the specified channel.
	 *
	 * Read \c numBytes bytes from the FPGA channel \c channel into the \c buffer array. Before
	 * calling this function you should verify that the FPGALink device actually supports CommFPGA
	 * using \c flIsCommCapable().
	 *
	 * Because this function is synchronous, it will block until the data has been returned. You
	 * must not use this function between an async read submit...await pair.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param channel The FPGA channel to read (0-127).
	 * @param numBytes The number of bytes to read.
	 * @param buffer The address of a buffer to store the bytes read from the FPGA.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_USB_ERR if a USB read or write error occurred.
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 */
	DLLEXPORT(FLStatus) flReadChannel(
		struct FLContext *handle, uint8 channel, size_t numBytes, uint8 *buffer,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Synchronously write one or more bytes to the specified channel.
	 *
	 * Write \c numBytes bytes from the \c sendData array to FPGA channel \c channel. Before calling
	 * this function you should verify that the FPGALink device actually supports CommFPGA using
	 * \c flIsCommCapable().
	 *
	 * Because this function is synchronous, it will block until the OS has confirmed that the data
	 * has been correctly sent over USB and received by the micro. It cannot confirm that the data
	 * has been received by the FPGA however: it may be waiting in the micro's output buffer.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param channel The FPGA channel to write (0-127).
	 * @param numBytes The number of bytes to write.
	 * @param sendData The address of the array of bytes to be written to the FPGA.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 *     - \c FL_USB_ERR if a USB write error occurred.
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 *     - \c FL_BAD_STATE if there are async reads in progress.
	 */
	DLLEXPORT(FLStatus) flWriteChannel(
		struct FLContext *handle, uint8 channel, size_t numBytes, const uint8 *sendData,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Set the chunk size to be used for future async writes.
	 *
	 * By default, the \c flWriteChannelAsync() function buffers up to 64KiB of data before sending
	 * anything over USB. Chunking the data in this way is more efficient than sending lots of
	 * little messages. However, the choice of chunk size affects the steady-state throughput in
	 * interesting ways. If you need to, you can choose to make the chunks smaller than 64KiB.
	 *
	 * You should not call this when there is some send data buffered. You should either call this
	 * before the first call to \c flWriteChannelAsync(), or call it immediately after a call to
	 * \c flFlushAsyncWrites().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param chunkSize The new chunksize in bytes. Passing zero sets the chunkSize to 64KiB.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_BAD_STATE if there is some outstanding send data.
	 */
	DLLEXPORT(FLStatus) flSetAsyncWriteChunkSize(
		struct FLContext *handle, uint16 chunkSize, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Asynchronously write one or more bytes to the specified channel.
	 *
	 * Write \c numBytes bytes from the \c sendData array to FPGA channel \c channel. Before calling
	 * this function you should verify that the FPGALink device actually supports CommFPGA using
	 * \c flIsCommCapable().
	 *
	 * This function is asynchronous. That means it will return immediately, usually before anything
	 * has been actually sent over USB. If the operation fails, you will not be notified of the
	 * failure until a future call to \c flAwaitAsyncWrites() or \c flReadChannelAsyncAwait(). The
	 * data is copied internally, so there's no need to worry about preserving the data: it's safe
	 * to call \c flWriteChannelAsync() on a stack-allocated array, for example.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param channel The FPGA channel to write (0-127).
	 * @param numBytes The number of bytes to write.
	 * @param sendData The address of the array of bytes to be written to the FPGA.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 *     - \c FL_USB_ERR if a USB write error occurred.
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 */
	DLLEXPORT(FLStatus) flWriteChannelAsync(
		struct FLContext *handle, uint8 channel, size_t numBytes, const uint8 *sendData,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Flush out any pending asynchronous writes.
	 *
	 * Flush any writes that have been buffered up, or do nothing if no writes have been buffered.
	 * This only triggers the send over USB; it does not guarantee the micro successfully received
	 * the data. See \c flAwaitAsyncWrites().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_USB_ERR if a USB write error occurred.
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 */
	DLLEXPORT(FLStatus) flFlushAsyncWrites(
		struct FLContext *handle, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Wait for confirmation that pending asynchronous writes were received by the micro.
	 *
	 * The first thing this does is to call \c flFlushAsyncWrites() to flush out any outstanding
	 * write commands. It will then block until the OS confirms that all the asynchronous write
	 * commands sent by \c flWriteChannelAsync() were correctly sent over USB and received by the
	 * micro. It cannot confirm that that the writes were received by the FPGA however: they may be
	 * waiting in the micro's output buffer.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_USB_ERR if one of the outstanding async operations failed.
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 *     - \c FL_BAD_STATE if there are async reads in progress.
	 */
	DLLEXPORT(FLStatus) flAwaitAsyncWrites(
		struct FLContext *handle, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Submit an asynchronous read of one or more bytes from the specified channel.
	 *
	 * Submit an asynchronous read of \c numBytes bytes from the FPGA channel \c channel. You can
	 * request at most 64KiB of data asynchronously. Before calling this function you should verify
	 * that the FPGALink device actually supports CommFPGA using \c flIsCommCapable().
	 *
	 * This function is asynchronous. That means it will return immediately, usually before the read
	 * request has been sent over USB. You will not find out the result of the read until you later
	 * call \c flReadChannelAsyncAwait() - this will give you your data, or tell you what went
	 * wrong.
	 *
	 * You should always ensure that for each call to \c flReadChannelAsyncSubmit(), there is a
	 * matching call to \c flReadChannelAsyncAwait(). You should not call any of
	 * \c flSetAsyncWriteChunkSize(), \c flAwaitAsyncWrites(), \c flWriteChannel() or
	 * \c flReadChannel() between a submit...await pair.
	 *
	 * USB host controllers typically need just one level of nesting of submit...await pairs to keep
	 * them busy. That means sequences like submit, submit, await, submit, await, submit, ...,
	 * await, await.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param channel The FPGA channel to read (0-127).
	 * @param numBytes The number of bytes to read, <= 64KiB (hence \c uint32 rather than
     *            \c size_t).
	 * @param buffer A buffer to receive the data, or \c NULL if you want to borrow one.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_USB_ERR if a USB read or write error occurred.
	 *     - \c FL_PROTOCOL_ERR if the device does not support CommFPGA.
	 */
	DLLEXPORT(FLStatus) flReadChannelAsyncSubmit(
		struct FLContext *handle, uint8 channel, uint32 numBytes, uint8 *buffer, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Await the data from a previously-submitted asynchronous read.
	 *
	 * Block until the outcome of a previous call to \c flReadChannelAsyncSubmit() is known. If the
	 * read was successful, you are given the resulting data. If not, an error code/message.
	 *
	 * On successful outcome, the three out parameters are populated with a pointer to the FPGA
	 * data, the requested length and the actual length. Unless the FPGA and micro support early
	 * termination of reads, the two lengths will be identical.
	 *
	 * Unless you provided your own buffer when you called \c flReadChannelAsyncSubmit(), the data
	 * returned is stored in an internal buffer. It is guaranteed to remain valid until your next
	 * call to any of the CommFPGA functions.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param recvData A pointer to a <code>const uint8 *</code> which will be set on exit to point
	 *            to a buffer containing the bytes read from the FPGA.
	 * @param requestLength A pointer to a \c uint32 which will be set on exit to the number of
	 *            bytes requested in the corresponding call to \c flReadChannelAsyncSubmit().
	 * @param actualLength A pointer to a \c uint32 which will be set on exit to the number of bytes
	 *            actually read from the FPGA.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_USB_ERR if one of the outstanding async operations failed.
	 */
	DLLEXPORT(FLStatus) flReadChannelAsyncAwait(
		struct FLContext *handle, const uint8 **recvData, uint32 *requestLength,
		uint32 *actualLength, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * Under some circumstances (e.g a Linux VM running on a Windows VirtualBox host talking to an
	 * FX2-based FPGALink device), it's necessary to manually reset the USB endpoints before
	 * doing any reads or writes. I admit this is hacky, and probably represents a bug somewhere.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_USB_ERR if a USB error occurred.
	 */
	DLLEXPORT(FLStatus) flResetToggle(
		struct FLContext *handle, const char **error
	) WARN_UNUSED_RESULT;
	//@}

	// ---------------------------------------------------------------------------------------------
	// NeroProg functions (only if flIsNeroCapable() returns true)
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name NeroProg Operations
	 * @{
	 */
	/**
	 * @brief Program a device using the specified file.
	 *
	 * This will program an FPGA or CPLD using the specified microcontroller ports and the specified
	 * programming file. Several programming algorithms are supported (JTAG, Xilinx Slave-Serial,
	 * Xilinx SelectMap and Altera Passive-Serial). In each case, it's necessary to tell the micro
	 * which ports to use. Here are some examples:
	 *
	 * A Digilent board using JTAG: <code>progConfig="J:D0D2D3D4"</code>:
	 * - TDO: PD0
	 * - TDI: PD2
	 * - TMS: PD3
	 * - TCK: PD4
	 *
	 * MakeStuff LX9 using JTAG: <code>progConfig="J:A7A0A3A1"</code>:
	 * - TDO: PA7
	 * - TDI: PA0
	 * - TMS: PA3
	 * - TCK: PA1
	 *
	 * EP2C5 Mini Board using Altera Passive-Serial: <code>progConfig="AS:B5B6B1B2"</code> (note
	 * that the board normally connects MSEL[1:0] to ground, hard-coding it in Active-Serial mode.
	 * For Passive-Serial to work you need to lift pin 85 and pull it up to VCC):
	 * - nCONFIG: PD5
	 * - CONF_DONE: PD6
	 * - DCLK: PD1
	 * - DATA0: PD2
	 *
	 * Aessent aes220 using Xilinx Slave-Serial:
	 * <code>progConfig="XS:D0D5D1D6A7[D3?,B1+,B5+,B3+]"</code>:
	 * - PROG_B: PD0
	 * - INIT_B: PD5
	 * - DONE: PD1
	 * - CCLK: PD6
	 * - DIN: PA7
	 * - Tristate DOUT (PD3)
	 * - Drive M[2:0]="111" (PB1, PB5, PB3) for Slave-Serial
	 *
	 * Aessent aes220 using Xilinx SelectMAP:
	 * <code>progConfig="XP:D0D5D1D6A01234567[B4-,D2-,D3?,B1+,B5+,B3-]"</code>:
	 * - PROG_B: PD0
	 * - INIT_B: PD5
	 * - DONE: PD1
	 * - CCLK: PD6
	 * - D[7:0]: PA[7:0]
	 * - Drive RDWR_B="0" (PB4)
	 * - Drive CSI_B="0" (PD2)
	 * - Tristate DOUT (PD3)
	 * - Drive M[2:0]="110" (PB1, PB5, PB3) for SelectMAP
	 *
	 * Note that this approach of specifying and implementing many disparate programming algorithms
	 * on the host side in terms of a much smaller set of building-block operations on the
	 * microcontroller is optimized for microcontrollers which support efficient remapping of I/O
	 * pins. For example the FX2 has a Von Neumann architecture where both code and data are stored
	 * in a single RAM-based address space, so port remapping can easily be achieved with
	 * self-modifying code. Conversely, the AVRs have Harvard architecture, where code and data are
	 * in separate address spaces, with code in flash so it cannot be self-modified. And actually,
	 * the AVR firmware is more likely to be tuned to a specific board layout than the more generic
	 * FX2 firmware.
	 *
	 * So, the bottom line is, even if you're using a microcontroller whose port pins are
	 * hard-coded, you still have to supply the port pins to use when you call functions expecting
	 * \c progConfig.
	 *
	 * You can either append the programming filename to the end of \c progConfig (e.g
	 * \c "J:A7A0A3A1:fpga.xsvf") or you can supply the programming filename separately in
	 * \c progFile.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param progConfig The port configuration described above.
	 * @param progFile The name of the programming file, or \c NULL if it's already given in
	 *            \c progConfig.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_ALLOC_ERR if we ran out of memory during programming.
	 *     - \c FL_USB_ERR if a USB error occurred.
	 *     - \c FL_FILE_ERR if the programming file is unreadable or an unexpected format.
	 *     - \c FL_UNSUPPORTED_CMD_ERR if an XSVF file contains an unsupported command.
	 *     - \c FL_UNSUPPORTED_DATA_ERR if an XSVF file contains an unsupported XENDDR/XENDIR.
	 *     - \c FL_UNSUPPORTED_SIZE_ERR if an XSVF command is too long.
	 *     - \c FL_SVF_PARSE_ERR if an SVF file is unparseable.
	 *     - \c FL_CONF_FORMAT if \c progConfig is malformed.
	 *     - \c FL_PROG_PORT_MAP if the micro was unable to map its ports to those given.
	 *     - \c FL_PROG_SEND if the micro refused to accept programming data.
	 *     - \c FL_PROG_RECV if the micro refused to provide programming data.
	 *     - \c FL_PROG_SHIFT if the micro refused to begin a JTAG shift operation.
	 *     - \c FL_PROG_JTAG_FSM if the micro refused to navigate the TAP state-machine.
	 *     - \c FL_PROG_JTAG_CLOCKS if the micro refused to send JTAG clocks.
	 *     - \c FL_PROG_SVF_COMPARE if an SVF/XSVF compare operation failed.
	 *     - \c FL_PROG_SVF_UNKNOWN_CMD if an SVF/XSVF unknown command was encountered.
	 *     - \c FL_PROG_ERR if the FPGA failed to start after programming.
	 *     - \c FL_PORT_IO if the micro refused to configure one of its ports.
	 */
	DLLEXPORT(FLStatus) flProgram(
		struct FLContext *handle, const char *progConfig, const char *progFile, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Program a device using the specified programming blob.
	 *
	 * This is similar to \c flProgram(), except that instead of reading the programming information
	 * from a file, it runs the programming operation from a binary blob already stored in memory.
	 * For JTAG programming this is assumed to be a CSVF file; for Xilinx programming it is assumed
	 * to be a raw bitstream (.bin) file.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param progConfig The port configuration described in \c flProgram().
	 * @param numBytes The number of bytes of programming data.
	 * @param progData A pointer to the start of the programming data.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_USB_ERR if a USB error occurred.
	 *     - \c FL_CONF_FORMAT if \c progConfig is malformed.
	 *     - \c FL_PROG_PORT_MAP if the micro was unable to map its ports to those given.
	 *     - \c FL_PROG_SEND if the micro refused to accept programming data.
	 *     - \c FL_PROG_RECV if the micro refused to provide programming data.
	 *     - \c FL_PROG_SHIFT if the micro refused to begin a JTAG shift operation.
	 *     - \c FL_PROG_JTAG_FSM if the micro refused to navigate the TAP state-machine.
	 *     - \c FL_PROG_JTAG_CLOCKS if the micro refused to send JTAG clocks.
	 *     - \c FL_PROG_SVF_COMPARE if an SVF/XSVF compare operation failed.
	 *     - \c FL_PROG_SVF_UNKNOWN_CMD if an SVF/XSVF unknown command was encountered.
	 *     - \c FL_PROG_ERR if the FPGA failed to start after programming.
	 *     - \c FL_PORT_IO if the micro refused to configure one of its ports.
	 */
	DLLEXPORT(FLStatus) flProgramBlob(
		struct FLContext *handle, const char *progConfig, uint32 numBytes, const uint8 *progData,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Scan the JTAG chain and return an array of IDCODEs.
	 *
	 * Count the number of devices on the JTAG chain, and set \c *numDevices accordingly. Then, if
	 * \c deviceArray is not \c NULL, populate it with at most \c arraySize IDCODEs, in chain order.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param portConfig The port bits to use for TDO, TDI, TMS & TCK, e.g "D0D2D3D4".
	 * @param numDevices A pointer to a \c uint32 which will be set on exit to the number of devices
	 *            in the JTAG chain.
	 * @param deviceArray A pointer to an array of \c uint32, which will be populated on exit with a
	 *            list of IDCODEs in chain order. May be \c NULL, in which case the function returns
	 *            after setting \c *numDevices.
	 * @param arraySize The number of 32-bit IDCODE slots available in \c deviceArray.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_CONF_FORMAT if \c portConfig is malformed.
	 *     - \c FL_PROG_PORT_MAP if the micro was unable to map its ports to those given.
	 *     - \c FL_PROG_SEND if the micro refused to accept programming data.
	 *     - \c FL_PROG_RECV if the micro refused to provide programming data.
	 *     - \c FL_PROG_SHIFT if the micro refused to begin a JTAG shift operation.
	 *     - \c FL_PROG_JTAG_FSM if the micro refused to navigate the TAP state-machine.
	 *     - \c FL_PORT_IO if the micro refused to configure one of its ports.
	 */
	DLLEXPORT(FLStatus) jtagScanChain(
		struct FLContext *handle, const char *portConfig,
		uint32 *numDevices, uint32 *deviceArray, uint32 arraySize,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Open an SPI/JTAG connection.
	 *
	 * Open a SPI/JTAG connection using the supplied \c portConfig. You must open a connection
	 * before calling \c jtagShiftInOut(), \c jtagShiftInOnly(), \c jtagClockFSM(), \c jtagClocks(),
	 * \c spiSend() or \c spiRecv(). And you must close the connection when you're finished, with
	 * \c progClose().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param portConfig The port bits to use for MISO(TDO), MOSI(TDI), SS(TMS) and SCK(TCK), e.g
	 *            "D0D2D3D4".
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_CONF_FORMAT if \c portConfig is malformed.
	 *     - \c FL_PROG_PORTMAP if the micro refused to map its ports to those given.
	 *     - \c FL_PORT_IO if the micro refused to configure one of its ports.
	 */
	DLLEXPORT(FLStatus) progOpen(
		struct FLContext *handle, const char *portConfig, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Close an SPI/JTAG connection.
	 *
	 * Close an SPI/JTAG connection previously opened by \c progOpen(), and tri-state the four
	 * programming pins.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_PORT_IO if the micro refused to configure one of its ports.
	 */
	DLLEXPORT(FLStatus) progClose(
		struct FLContext *handle, const char **error
	) WARN_UNUSED_RESULT;

	// Special values for inData parameter of jtagShiftInOut() declared below
	// @cond NEVER
	#define SHIFT_ZEROS ((const uint8*)0)
	#define SHIFT_ONES ((const uint8*)-1)
	// @endcond

	/**
	 * @brief Shift data into the JTAG state-machine.
	 *
	 * Shift \c numBits bits LSB-first from \c tdiData into TDI. If \c isLast is zero, leave the
	 * TAP state-machine in \c Shift-xR, otherwise exit to \c Exit1-xR on the final bit. If you want
	 * \c tdiData to be all zeros you can use \c SHIFT_ZEROS, or if you want it to be all ones you
	 * can use \c SHIFT_ONES. This is more efficient than explicitly sending an array containing all
	 * zeros or all 0xFFs.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param numBits The number of bits to clock into the JTAG state-machine.
	 * @param tdiData A pointer to the source data, or \c SHIFT_ZEROS or \c SHIFT_ONES.
	 * @param isLast Either 0 to remain in \c Shift-xR, or 1 to exit to \c Exit1-xR.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_PROG_SEND if the micro refused to accept programming data.
	 *     - \c FL_PROG_SHIFT if the micro refused to begin a JTAG shift operation.
	 */
	DLLEXPORT(FLStatus) jtagShiftInOnly(
		struct FLContext *handle, uint32 numBits, const uint8 *tdiData, uint8 isLast,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Shift data into and out of the JTAG state-machine.
	 *
	 * Shift \c numBits bits LSB-first from \c tdiData into TDI; at the same time shift the same
	 * number of bits LSB-first from TDO into \c tdoData. If \c isLast is zero, leave the TAP
	 * state-machine in \c Shift-xR, otherwise exit to \c Exit1-xR on the final bit. If you want
	 * \c tdiData to be all zeros you can use \c SHIFT_ZEROS, or if you want it to be all ones you
	 * can use \c SHIFT_ONES. This is more efficient than explicitly sending an array containing all
	 * zeros or all 0xFFs.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param numBits The number of bits to clock into and out of the JTAG state-machine.
	 * @param tdiData A pointer to the source data, or \c SHIFT_ZEROS or \c SHIFT_ONES.
	 * @param tdoData A pointer to a buffer to receive output data, or \c NULL if you don't care.
	 * @param isLast Either 0 to remain in \c Shift-xR, or 1 to exit to \c Exit1-xR.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_PROG_SEND if the micro refused to accept programming data.
	 *     - \c FL_PROG_RECV if the micro refused to provide programming data.
	 *     - \c FL_PROG_SHIFT if the micro refused to begin a JTAG shift operation.
	 */
	DLLEXPORT(FLStatus) jtagShiftInOut(
		struct FLContext *handle, uint32 numBits, const uint8 *tdiData, uint8 *tdoData,
		uint8 isLast, const char **error
	) WARN_UNUSED_RESULT;
	
	/**
	 * @brief Clock \c transitionCount bits from \c bitPattern into TMS, starting with the LSB.
	 *
	 * Navigate the TAP state-machine by clocking an arbitrary sequence of bits into TMS.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param bitPattern The pattern of bits to clock into TMS, LSB first.
	 * @param transitionCount The number of bits to clock.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_PROG_JTAG_FSM if the micro refused to navigate the TAP state-machine.
	 */
	DLLEXPORT(FLStatus) jtagClockFSM(
		struct FLContext *handle, uint32 bitPattern, uint8 transitionCount, const char **error
	) WARN_UNUSED_RESULT;
	
	/**
	 * @brief Toggle TCK \c numClocks times.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param numClocks The number of clocks to put out on TCK.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_PROG_JTAG_CLOCKS if the micro refused to send JTAG clocks.
	 */
	DLLEXPORT(FLStatus) jtagClocks(
		struct FLContext *handle, uint32 numClocks, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Get the physical port number of the specified logical port.
	 *
	 * Get the physical port number assigned to the specified logical port by the preceding call to
	 * \c progOpen(). This is just a convenience function to avoid re-parsing the port config, which
	 * is typically supplied by the user as a string. For example, to send data to a SPI peripheral,
	 * you'll probably want to assert \c SS. So you'll want to call \c progGetPort(handle, LP_SS) to
	 * find out which physical port \c SS was assigned to.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param logicalPort The @ref LogicalPort to query for.
	 * @returns The physical port mapped to the given @ref LogicalPort.
	 */
	DLLEXPORT(uint8) progGetPort(struct FLContext *handle, uint8 logicalPort);

	/**
	 * @brief Get the physical bit number of the specified logical port.
	 *
	 * Get the physical bit number assigned to the specified logical port by the preceding call to
	 * \c progOpen(). This is just a convenience function to avoid re-parsing the port config, which
	 * is typically supplied by the user as a string. For example, to send data to a SPI peripheral,
	 * you'll probably want to assert \c SS. So you'll want to call \c progGetBit(handle, LP_SS) to
	 * find out which physical port bit \c SS was assigned to.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param logicalPort The @ref LogicalPort to query for.
	 * @returns The physical bit mapped to the given @ref LogicalPort.
	 */
	DLLEXPORT(uint8) progGetBit(struct FLContext *handle, uint8 logicalPort);

	/**
	 * @brief Send a number of whole bytes over SPI, either LSB-first or MSB-first.
	 *
	 * Shift \c numBytes bytes from \c sendData into the microcontroller's SPI bus (if any), either
	 * MSB-first or LSB-first. You must have previously called \c progOpen().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param numBytes The number of bytes to send.
	 * @param sendData A pointer to the source data.
	 * @param bitOrder Either \c SPI_MSBFIRST or \c SPI_LSBFIRST (see @ref BitOrder).
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 *     - \c FL_USB_ERR if USB communications failed whilst sending the data.
	 *     - \c FL_PROTOCOL_ERR if the device does not support SPI.
	 */
	DLLEXPORT(FLStatus) spiSend(
		struct FLContext *handle, uint32 numBytes, const uint8 *sendData, uint8 bitOrder,
		const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Receive a number of whole bytes over SPI, either LSB-first or MSB-first.
	 *
	 * Shift \c numBytes bytes from the microcontroller's SPI bus (if any) into \c buffer, either
	 * MSB-first or LSB-first. You must have previously called \c progOpen().
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param buffer A pointer to a buffer to receive the data.
	 * @param numBytes The number of bytes to receive.
	 * @param bitOrder Either \c SPI_MSBFIRST or \c SPI_LSBFIRST (see @ref BitOrder).
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the operation completed successfully.
	 *     - \c FL_USB_ERR if USB communications failed whilst receiving the data.
	 *     - \c FL_PROTOCOL_ERR if the device does not support SPI.
	 */
	DLLEXPORT(FLStatus) spiRecv(
		struct FLContext *handle, uint32 numBytes, uint8 *buffer, uint8 bitOrder, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Swap the bits in a byte array.
	 *
	 * Swap the bits in a byte, so that 0x01 -> 0x80, 0x02 -> 0x40 etc.
	 *
	 * @param numBytes The number of bytes to be bit-swapped.
	 * @param data A pointer to an array of \c uint8 to be bit-swapped.
	 */
	DLLEXPORT(void) spiBitSwap(uint32 numBytes, uint8 *data);
	//@}

	// ---------------------------------------------------------------------------------------------
	// Firmware functions
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name Firmware Operations
	 * @{
	 */
	/**
	 * @brief Load standard FPGALink firmware into the FX2's RAM.
	 *
	 * Load the FX2 chip at the "current" VID:PID with a precompiled firmware such that it will
	 * renumerate as the "new" VID:PID. The firmware is loaded into RAM, so the change is not
	 * permanent. Typically after calling \c flLoadStandardFirmware() applications should wait for
	 * the renumeration to complete by calling \c flIsDeviceAvailable() repeatedly until the "new"
	 * VID:PID becomes active.
	 *
	 * @param curVidPid The current Vendor/Product (i.e VVVV:PPPP) of the FX2 device.
	 * @param newVidPid The Vendor/Product/Device (i.e VVVV:PPPP:DDDD) that you \b want the FX2
	 *            device to renumerate as.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 *     - \c FL_USB_ERR if one of the VID:PIDs was invalid or the current VID:PID was not found.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2.
	 */
	DLLEXPORT(FLStatus) flLoadStandardFirmware(
		const char *curVidPid, const char *newVidPid, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Flash standard FPGALink firmware into the FX2's EEPROM.
	 *
	 * @warning This function will make permanent changes to your hardware. Remember to make a
	 * backup copy of the existing EEPROM firmware with \c flSaveFirmware() before calling it.
	 *
	 * Overwrite the FX2's EEPROM with a precompiled FPGALink firmware such that the board will
	 * enumerate on power-on as the "new" VID:PID.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param newVidPid The Vendor/Product (i.e VVVV:PPPP) you want the FX2 to be on power-on.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware flashed successfully.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 *     - \c FL_USB_ERR if the VID:PID was invalid.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2.
	 */
	DLLEXPORT(FLStatus) flFlashStandardFirmware(
		struct FLContext *handle, const char *newVidPid, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Load custom firmware (<code>.hex</code>) into the FX2's RAM.
	 *
	 * Load the FX2 chip at the given VID:PID with a <code>.hex</code> firmware file. The firmware
	 * is loaded into RAM, so the change is not permanent.
	 *
	 * @param curVidPid The current Vendor/Product (i.e VVVV:PPPP) of the FX2 device.
	 * @param fwFile A <code>.hex</code> file containing new FX2 firmware to be loaded into the
	 *            FX2's RAM.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 *     - \c FL_USB_ERR if the VID:PID was invalid.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2.
	 *     - \c FL_FILE_ERR if \c fwFile has a bad extension or could not be loaded.
	 */
	DLLEXPORT(FLStatus) flLoadCustomFirmware(
		const char *curVidPid, const char *fwFile, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Flash a custom firmware from a file into the FX2's EEPROM.
	 *
	 * @warning This function will make permanent changes to your hardware. Remember to make a
	 * backup copy of the existing EEPROM firmware with \c flSaveFirmware() before calling it.
	 *
	 * Overwrite the FX2's EEPROM with a custom firmware from a <code>.hex</code> or
	 * <code>.iic</code> file.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param fwFile A <code>.hex</code> or <code>.iic</code> file containing new FX2 firmware to be
	 *            loaded into the FX2's EEPROM.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2.
	 *     - \c FL_FILE_ERR if the firmware file could not be loaded.
	 */
	DLLEXPORT(FLStatus) flFlashCustomFirmware(
		struct FLContext *handle, const char *fwFile, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Save existing EEPROM data to a file.
	 *
	 * The existing EEPROM firmware is saved to an <code>.iic</code> file, for backup purposes.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param eepromSize The size in kilobits of the EEPROM (e.g Nexys2's EEPROM is 128kbit).
	 * @param saveFile An <code>.iic</code> file to save the EEPROM to.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the firmware loaded successfully.
	 *     - \c FL_ALLOC_ERR if there was a memory allocation failure.
	 *     - \c FL_FX2_ERR if there was a problem talking to the FX2.
	 *     - \c FL_FILE_ERR if \c saveFile file could not be written.
	 */
	DLLEXPORT(FLStatus) flSaveFirmware(
		struct FLContext *handle, uint32 eepromSize, const char *saveFile, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Put the AVR in DFU bootloader mode.
	 *
	 * This is an AVR-specific utility function to make firmware upgrades easier on boards on which
	 * the /HWB and /RESET pins are not easily accesible. The "gordon" utility has an option to
	 * invoke this.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the port access command completed successfully.
	 *     - \c FL_USB_ERR if the device is not running suitable FPGALink/AVR firmware.
	 */
	DLLEXPORT(FLStatus) flBootloader(
		struct FLContext *handle, const char **error
	) WARN_UNUSED_RESULT;
	//@}

	// ---------------------------------------------------------------------------------------------
	// Utility functions
	// ---------------------------------------------------------------------------------------------
	/**
	 * @name Utility Functions
	 * @{
	 */
	/**
	 * @brief Sleep for the specified number of milliseconds.
	 *
	 * @param ms The number of milliseconds to sleep.
	 */
	DLLEXPORT(void) flSleep(
		uint32 ms
	);

	/**
	 * @brief Return a newly-allocated buffer with the specified file loaded into it.
	 *
	 * The specified file is queried for its length, that length is written to the \c numBytes
	 * parameter. Then a buffer of that length is allocated, and the file is loaded into it and a
	 * pointer to the buffer returned. The responsibility for the buffer passes to the caller; it
	 * must be freed later by a call to \c flFreeFile().
	 *
	 * @param name The name of the file to load.
	 * @param numBytes A pointer to a \c size_t which will be populated with the file's length.
	 * @returns A pointer to the allocated buffer, or \c NULL if the file could not be loaded.
	 */
	DLLEXPORT(uint8*) flLoadFile(
		const char *name, size_t *numBytes
	);

	/**
	 * @brief Free a buffer previously returned by \c flLoadFile().
	 * @param buffer A buffer previously returned by \c flLoadFile().
	 */
	DLLEXPORT(void) flFreeFile(
		uint8 *buffer
	);

	/**
	 * @brief Configure a single port bit on the microcontroller.
	 *
	 * With this function you can set a single microcontroller port bit to one of the enums in
	 * @ref PinConfig, and read back the current state of the bit.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param portNumber Which port to configure (i.e 0=PortA, 1=PortB, 2=PortC, etc).
	 * @param bitNumber The bit within the chosen port to use.
	 * @param pinConfig Either \c PIN_INPUT, \c PIN_HIGH or \c PIN_LOW.
	 * @param pinRead Pointer to a <code>uint8</code> to be set on exit to 0 or 1 depending on
	 *            the current state of the pin. May be \c NULL if you're not interested.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the port access command completed successfully.
	 *     - \c FL_PORT_IO if the micro failed to respond to the port access command.
	 */
	DLLEXPORT(FLStatus) flSingleBitPortAccess(
		struct FLContext *handle, uint8 portNumber, uint8 bitNumber,
		uint8 pinConfig, uint8 *pinRead, const char **error
	) WARN_UNUSED_RESULT;

	/**
	 * @brief Configure multiple port bits on the microcontroller.
	 *
	 * With this function you can set multiple microcontroller port bits to either \c PIN_INPUT,
	 * \c PIN_HIGH or \c PIN_LOW, and read back the current state of each bit. This is achieved by
	 * sending a comma-separated list of port configurations, e.g "A12-,B2+,C7?". A "+" or a "-"
	 * suffix sets the port as an output, driven high or low respectively, and a "?" suffix sets the
	 * port as an input. The current state of up to 32 bits are returned in \c readState, LSB first.
	 *
	 * @param handle The handle returned by \c flOpen().
	 * @param portConfig A comma-separated sequence of port configurations.
	 * @param readState Pointer to a <code>uint32</code> to be set on exit to the port readback.
	 * @param error A pointer to a <code>const char*</code> which will be set on exit to an
	 *            allocated error message if something goes wrong. Responsibility for this
	 *            allocated memory passes to the caller and must be freed with \c flFreeError(). If
	 *            \c error is \c NULL, no allocation is done and no message is returned, but the
	 *            return code will still be valid.
	 * @returns
	 *     - \c FL_SUCCESS if the port access command completed successfully.
	 *     - \c FL_CONF_FORMAT if \c portConfig is malformed.
	 *     - \c FL_PORT_IO if the micro failed to respond to the port access command.
	 */
	DLLEXPORT(FLStatus) flMultiBitPortAccess(
		struct FLContext *handle, const char *portConfig, uint32 *readState, const char **error
	) WARN_UNUSED_RESULT;
	//@}

#ifdef __cplusplus
}
#endif

#endif
