cmake_minimum_required(VERSION 3.10)
project(marcconv)

# Setting C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_COMPILER gcc)

add_definitions(-D_FILE_OFFSET_BITS=64)

include_directories(.)

# Compiler options
add_compile_options(-Wall)
add_compile_options(-O0)
#add_compile_options(-m32)

# To strip the produced executable
set(CMAKE_C_FLAGS_RELEASE "-s")

# List source files to be compiled
set(SOURCE_FILES
        marcconv.c marcproc.c marcproclist.c amuopt.c istrstr.c
        meshproc.c custombib.c mrv_util.c marcinit.c marcnew.c marcfree.c
        marcaddf.c marcadds.c marcordf.c marcords.c marcgetr.c marcgetf.c
        marcgets.c marcgeti.c marcposf.c marcposs.c marcnxtf.c marcdup.c
        marcnxts.c marcnxti.c marcold.c marccur.c marccoll.c marcindc.c
        marcdelf.c marcdels.c marcref.c marcrdwr.c marccpyf.c marcoksf.c
        marcrenf.c marcrens.c marcsave.c marcxsfd.c marcxchk.c marcxnum.c
        marcxalo.c marcxcdt.c
)

set(LIBOBJ
        marcinit.c marcnew.c marcfree.c marcaddf.c marcadds.c
        marcordf.c marcords.c marcgetr.c marcgetf.c marcgets.c
        marcgeti.c marcposf.c marcposs.c marcnxtf.c marcdup.c
        marcnxts.c marcnxti.c marcold.c marccur.c marccoll.c
        marcindc.c marcdelf.c marcdels.c marcref.c marcrdwr.c
        marccpyf.c marcoksf.c marcrenf.c marcrens.c marcsave.c
        marcxsfd.c marcxchk.c marcxnum.c marcxalo.c marcxcdt.c
)

set(DEPS
        marc.h marcconv.h marcproclist.h mrv_util.h
)

set(OBJ
        marcconv.c marcproc.c marcproclist.c amuopt.c istrstr.c meshproc.c custombib.c mrv_util.c
)

add_executable(${PROJECT_NAME} ${OBJ} ${LIBOBJ})
