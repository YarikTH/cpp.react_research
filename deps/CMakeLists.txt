
set(TBB_BUILD_SHARED          n CACHE BOOL "Build TBB shared library" FORCE)
set(TBB_BUILD_STATIC          y CACHE BOOL "Build TBB static library" FORCE)
set(TBB_BUILD_TBBMALLOC       n CACHE BOOL "Build TBB malloc library" FORCE)
set(TBB_BUILD_TBBMALLOC_PROXY n CACHE BOOL "Build TBB malloc proxy library" FORCE)
set(TBB_BUILD_TESTS           n CACHE BOOL "Build TBB tests and enable testing infrastructure" FORCE)
set(TBB_NO_DATE               y CACHE BOOL "Do not save the configure date in the version string" FORCE)
set(TBB_BUILD_PYTHON          n CACHE BOOL "Build TBB Python bindings" FORCE)
set(TBB_SET_SOVERSION         n CACHE BOOL "Set the SOVERSION (shared library build version suffix)?" FORCE)
set(TBB_INSTALL_TARGETS       n CACHE BOOL "Include build targets for 'make install'" FORCE)

add_subdirectory(tbb)


add_subdirectory(googletest)
