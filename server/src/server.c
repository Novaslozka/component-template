// standard libraries
#include <string.h>

// SALLY libraries
#include <service/service.h>
#include <service/service_filesystem.h>
#include <service/service_com.h>

// SALLY backends - set by CMake
extern service_fs_t FS_BACKEND;
service_fs_t* fs = &FS_BACKEND;
extern service_os_t OS_BACKEND;
service_os_t* os = &OS_BACKEND;
extern service_com_t COM_BACKEND;
service_com_t* com_tty = &COM_BACKEND;

void thread_test(void* arg) {
  // FS example
  fs->init();
  service_fs_file_handle_t hfile = fs->file_handle_malloc();
  fs->open(hfile, "testfile.txt", fs->mode.WRONLY | fs->mode.CREAT | fs->mode.APPEND);
  fs->write(hfile, (char*)arg, strlen(arg));
  fs->close(hfile);
  fs->file_handle_free(hfile);

  // COM example
  service_com_handle_t hcom = com_tty->com_handle_malloc();
  const char* port = "dev/ttyS0";
  service_com_conf_t conf = {
    .speed = 460800,
  };
  com_tty->open(hcom, (service_com_device_t)port, conf);
  com_tty->write(hcom, (char*)arg, strlen(arg));
  com_tty->close(hcom);
  com_tty->com_handle_free(hcom);
}

int main(int argc, char *argv[]) {
  // OS example
  service_os_thread_handle_t thread;
  const char* text = "Hello World!\n";
  os->thread_create(&thread, NULL, thread_test, (void*)text);
  os->thread_wait_for(thread);
  return(0);
}
