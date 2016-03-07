/*BINFMTCXX: -std=c++11 -Wall -Werror
*/

#include <spawn.h> // see manpages-posix-dev
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

int main()
{
  int exit_code;
  int cout_pipe[2];
  int cerr_pipe[2];
  posix_spawn_file_actions_t action;

  if(pipe(cout_pipe) || pipe(cerr_pipe))
    cout << "pipe returned an error.\n";

  posix_spawn_file_actions_init(&action);
  posix_spawn_file_actions_addclose(&action, cout_pipe[0]);
  posix_spawn_file_actions_addclose(&action, cerr_pipe[0]);
  posix_spawn_file_actions_adddup2(&action, cout_pipe[1], 1);
  posix_spawn_file_actions_adddup2(&action, cerr_pipe[1], 2);

  posix_spawn_file_actions_addclose(&action, cout_pipe[1]);
  posix_spawn_file_actions_addclose(&action, cerr_pipe[1]);

  string command = "/usr/bin/which ping";
  string argsmem[] = {"bash","-l","-c"}; // allows non-const access to literals
  char * args[] = {&argsmem[0][0],&argsmem[1][0],&argsmem[2][0],&command[0],nullptr};
  //char * args[] = {&command[0],nullptr};

  pid_t pid;
  if(posix_spawnp(&pid, args[0], &action, NULL, &args[0], NULL) != 0)
    cout << "posix_spawnp failed with error: " << strerror(errno) << "\n";

  close(cout_pipe[1]), close(cerr_pipe[1]); // close child-side of pipes

  // Read from pipes
  string buffer(1024,' ');
  /*
  std::vector<pollfd> plist = { {cout_pipe[0],POLLIN}, {cerr_pipe[0],POLLIN} };
  for ( int rval; (rval=poll(&plist[0],plist.size(),-1))>0; ) {
    if ( plist[0].revents&POLLIN) {
      int bytes_read = read(cout_pipe[0], &buffer[0], buffer.length());
      cout << "read " << bytes_read << " bytes from stdout.\n";
      cout << buffer.substr(0, static_cast<size_t>(bytes_read)) << "\n";
    }
    else if ( plist[1].revents&POLLIN ) {
      int bytes_read = read(cerr_pipe[0], &buffer[0], buffer.length());
      cout << "read " << bytes_read << " bytes from stderr.\n";
      cout << buffer.substr(0, static_cast<size_t>(bytes_read)) << "\n";
    }
    else break; // nothing left to read
  }
  */

  struct timespec timeout = {5, 0};

  fd_set read_set;
  memset(&read_set, 0, sizeof(read_set));
  FD_SET(cout_pipe[0], &read_set);
  FD_SET(cerr_pipe[0], &read_set);

  int larger_fd = (cout_pipe[0] > cerr_pipe[0]) ? cout_pipe[0] : cerr_pipe[0];

  int rc = pselect(larger_fd + 1, &read_set, NULL, NULL, &timeout, NULL);
  //thread blocks until either packet is received or the timeout goes through
  if (rc == 0)
  {
      std::cout << "Timed out." << std::endl;
      return 1;
  }

  int bytes_read = read(cerr_pipe[0], &buffer[0], buffer.length());
  if (bytes_read > 0)
  {
    std::cout << "Got error message: " << buffer.substr(0, bytes_read) << std::endl;
  }

    bytes_read = read(cout_pipe[0], &buffer[0], buffer.length());
  if (bytes_read > 0)
  {
    //std::cout << "Read in " << bytes_read << " bytes from cout_pipe." << std::endl;
    std::cout << buffer.substr(0, bytes_read) << std::endl;
  }

  waitpid(pid,&exit_code,0);
  cout << "exit code: " << exit_code << "\n";

  posix_spawn_file_actions_destroy(&action);
}