
## process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = move_shmem_pages 
noinst_PROGRAMS = create_shmem unlink_shmem shmem_numainfo

move_shmem_pages_SOURCES = \
  src/move_shmem_pages.c src/move_shmem_pages.h
move_shmem_pages_LDADD = -lrt -lnuma

create_shmem_SOURCES = \
  src/create_shmem.c src/create_shmem.h
create_shmem_LDADD = -lrt

unlink_shmem_SOURCES = \
  src/unlink_shmem.c src/unlink_shmem.h
unlink_shmem_LDADD = -lrt

shmem_numainfo_SOURCES = \
  src/shmem_numainfo.c src/shmem_numainfo.h
shmem_numainfo_LDADD = -lrt -lnuma
