# Shared subset from pinned rcheevos v12.5.0. External integration and other
# platform hash formats are not enabled. RC_NO_THREADS requires one owner.
RC_ROOT ?= ../../vendor/rcheevos
RC_DEFINES = -DRC_NO_THREADS -DRC_HASH_NO_ROM -DRC_HASH_NO_ZIP -DRC_HASH_NO_ENCRYPTED
RC_SOURCES = $(RC_ROOT)/src/rc_client.c $(RC_ROOT)/src/rc_compat.c $(RC_ROOT)/src/rc_util.c $(RC_ROOT)/src/rc_version.c $(wildcard $(RC_ROOT)/src/rcheevos/*.c) $(wildcard $(RC_ROOT)/src/rapi/*.c) $(RC_ROOT)/src/rhash/md5.c $(RC_ROOT)/src/rhash/hash.c $(RC_ROOT)/src/rhash/hash_disc.c $(RC_ROOT)/src/rhash/cdreader.c
RC_INCLUDES = -I$(RC_ROOT)/include -I$(RC_ROOT)/src/rhash
