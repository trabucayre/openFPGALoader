FIND_PROGRAM(GZIP_PRG
	NAMES gzip
	PATHS /bin
		/usr/bin
		/usr/local/bin
		${CYGWIN_INSTALL_PATH}/bin
		${MSYS_INSTALL_PATH}/usr/bin
)

IF(NOT GZIP_PRG)
  message("Unable to find 'gzip' program")
ENDIF(NOT GZIP_PRG)

