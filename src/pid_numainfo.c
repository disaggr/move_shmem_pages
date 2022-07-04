
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "pid_numainfo.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <numaif.h>
#include <stdint.h>

struct arguments_t arguments = {
  pid: 0};

void
usage ()
{
  fprintf(stderr, "usage: %s <pid>\n",
    program_invocation_name);
}

int
main (int argc, char *argv[])
{
  // parse arguments
  if (argc < 2 || argc > 2)
  {
    usage();
    return 2;
  }

  // argument 1: tracee pid
  arguments.pid = strtol(argv[1], NULL, 0);

  // read /proc/<pid>/maps
  // TODO: can I get this information through API?
  size_t buflen = snprintf(NULL, 0, "/proc/%lu/maps", arguments.pid);
  char *path = malloc(sizeof(*path) * buflen + 1);
  if (path == NULL)
  {
    fprintf(stderr, "%s: error: failed to allocate memory: %s\n",
      program_invocation_name, strerror(errno));
    return 1;
  }
  snprintf(path, buflen + 1, "/proc/%lu/maps", arguments.pid);
  int fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    fprintf(stderr, "%s: error: %s: open: %s\n",
      program_invocation_name, path, strerror(errno));
    return 1;
  }

  char *map_data = NULL;
  size_t map_size = 0;
  size_t data_size = 0;
  const size_t increment = 1024 * 4;
  char *wpt = NULL;

  while (1)
  {
    map_size = data_size + increment;
    map_data = realloc(map_data, map_size);
    if (map_data == NULL)
    {
      fprintf(stderr, "%s: error: failed to allocate memory: %s\n",
        program_invocation_name, strerror(errno));
      return 1;
    }
    wpt = map_data + data_size;
    ssize_t res = read(fd, wpt, increment);
    if (res < 0)
    {
      fprintf(stderr, "%s: error: %s: read: %s\n",
        program_invocation_name, path, strerror(errno));
      return 1;
    }
    if (res == 0)
    {
      *wpt = 0;
      break;
    }
    data_size += res;

    //printf("%.5s  %zx  %zx  %zx\n", wpt, map_size, data_size, wpt);
  }

  size_t i;
  for (i = 0; i < data_size; ++i)
    if (map_data[i] == '\n')
      map_data[i] = '\0';

  printf("  page: 0x%012zx bytes (%zu KiB)\n\n",
    sysconf(_SC_PAGESIZE), sysconf(_SC_PAGESIZE) / 1024);

  int num_read;

  const char *rpt = map_data;
  while (rpt < map_data + data_size)
  {
    uintptr_t start = 0;
    uintptr_t end = 0;
    char perms[5] = { 0 };
    off_t offset = 0;
    char dev[6] = { 0 };
    size_t inode = 0;
    const char *map_path = NULL;

    //printf("-%.50s...\n", rpt);
    int res = sscanf(rpt, "%lx-%lx %4s %lx %5s %lx%n",
      &start, &end, perms, &offset, dev, &inode, &num_read);
    rpt += num_read;
    map_path = rpt;
    while (*map_path == ' ')
      map_path++;

    //printf("+%lx-%lx %4s %lx %5s %lx %s\n", start, end, perms, offset, dev, inode, map_path);

    if (res == 0)
      break;

    while (*rpt != '\0')
      rpt++;
    rpt++;

    // determine placement
    size_t num_pages = (end - start) / sysconf(_SC_PAGESIZE);
    if ((end - start) % sysconf(_SC_PAGESIZE) > 0)
      num_pages += 1;

    void **pages = malloc(sizeof(*pages) * num_pages);
    int *status = malloc(sizeof(*status) * num_pages);

    if (pages == NULL || status == NULL)
    {
      fprintf(stderr, "%s: error: failed to allocate memory: %s\n",
        program_invocation_name, strerror(errno));
      return 1;
    }

    size_t i;
    for (i = 0; i < num_pages; ++i)
    {
      pages[i] = ((void*)start) + i * sysconf(_SC_PAGESIZE);
      status[i] = 0;
      // touch the page once, to make move_pages happy
      //(void)*(volatile int*)pages[i];
    }

    res = move_pages(arguments.pid, num_pages, pages, NULL, status, MPOL_MF_MOVE_ALL);
    if (res != 0)
    {
      fprintf(stderr, "%s: error: %lu: failed to determine placement: %s\n",
        program_invocation_name, arguments.pid, strerror(errno));
      return 1;
    }

    // print placement information
    printf("  0x%012lx-0x%012lx %s\n", start, end, map_path);
    int region_start = 0;
    for (i = 1; i < num_pages; ++i)
    {
      if (status[i] != status[region_start])
      {
        printf("  - 0x%012lx ... 0x%012lx\t[%lu]\t%i",
          (off_t)(pages[region_start]),
          (off_t)(pages[i - 1] + sysconf(_SC_PAGESIZE) - 1),
          (pages[i - 1] + sysconf(_SC_PAGESIZE) - pages[region_start]) / sysconf(_SC_PAGESIZE),
          status[region_start]);
        if (status[region_start] < 0)
        {
          printf(" (%s)", strerror(-status[region_start]));
        }
        printf("\n");
        region_start = i;
      }
    }
    printf("  - 0x%012lx ... 0x%012lx\t[%lu]\t%i",
      (off_t)(pages[region_start]),
      (off_t)(pages[num_pages - 1] + sysconf(_SC_PAGESIZE) - 1),
      (pages[i - 1] + sysconf(_SC_PAGESIZE) - pages[region_start]) / sysconf(_SC_PAGESIZE),
      status[region_start]);
    if (status[region_start] < 0)
    {
      printf(" (%s)", strerror(-status[region_start]));
    }
    printf("\n");
  }

  return 0;
}
