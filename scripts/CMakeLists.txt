IF(INSTALL_MISC_SCRIPTS)
  INSTALL(PROGRAMS reldistplot.r
    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
    )
ENDIF()
