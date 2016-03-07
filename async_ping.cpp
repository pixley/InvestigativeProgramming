#include <spawn.h> // see manpages-posix-dev
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <iostream>
#include <pthread.h>
#include <time.h>
#include <vector>
#include <sstream>

struct thread_info
{
	bool& complete;
	std::string& buf;

	thread_info(bool& comp, std::string& buffer) : complete(comp), buf(buffer) {};
};

std::string which_ping()
{
	int exit_code;
	int cout_pipe[2];
	int cerr_pipe[2];
	posix_spawn_file_actions_t action;

	if(pipe(cout_pipe) || pipe(cerr_pipe))
    {
    	std::cout << "pipe returned an error." << std::endl;
    	return "";
    }

	posix_spawn_file_actions_init(&action);
	posix_spawn_file_actions_addclose(&action, cout_pipe[0]);
	posix_spawn_file_actions_addclose(&action, cerr_pipe[0]);
	posix_spawn_file_actions_adddup2(&action, cout_pipe[1], 1);
	posix_spawn_file_actions_adddup2(&action, cerr_pipe[1], 2);

	posix_spawn_file_actions_addclose(&action, cout_pipe[1]);
	posix_spawn_file_actions_addclose(&action, cerr_pipe[1]);

	std::string command = "/usr/bin/which ping";
	std::string argsmem[] = {"bash","-l","-c"}; // allows non-const access to literals
	char * args[] = {&argsmem[0][0],&argsmem[1][0],&argsmem[2][0],&command[0],nullptr};

	pid_t pid;
	if(posix_spawnp(&pid, args[0], &action, NULL, &args[0], NULL) != 0)
	{
    	std::cout << "posix_spawnp failed with error: " << strerror(errno) << "\n";
    	exit(-2);
    }

    // close child-side of pipes
    close(cout_pipe[1]);
    close(cerr_pipe[1]);

    //std::cout << "Started 'which' process, PID is " << pid << std::endl;

    std::string buf(64,' ');

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
        return "";
    }

	int bytes_read = read(cerr_pipe[0], &buf[0], buf.length());
	if (bytes_read > 0)
	{
		std::cout << "Got error message: " << buf.substr(0, bytes_read) << std::endl;
		return "";
	}

    bytes_read = read(cout_pipe[0], &buf[0], buf.length());
	if (bytes_read > 0)
	{
		//std::cout << "Read in " << bytes_read << " bytes from cout_pipe." << std::endl;
	}
	else
	{
		std::cout << "Read nothing from cout_pipe." << std::endl;
	}

	waitpid(pid,&exit_code,0);

	posix_spawn_file_actions_destroy(&action);

    return buf.substr(0,bytes_read - 1); //trim off the trailing newline
}

void* read_pipe(void* info)
{
	struct thread_info* t_info = (struct thread_info*)info;

	int exit_code;
	int cout_pipe[2];
	int cerr_pipe[2];
	posix_spawn_file_actions_t action;

	if(pipe(cout_pipe) || pipe(cerr_pipe))
	{
		std::cout << "pipe returned an error.\n";
		exit(-1);
	}

	std::string ping_path = which_ping();

	if (ping_path.length() == 0)
	{
		std::cout << "Was unable to find ping executable." << std::endl;
		exit(-7);
	}

	std::cout << "Ping path: " << ping_path << std::endl;

	posix_spawn_file_actions_init(&action);
	posix_spawn_file_actions_addclose(&action, cout_pipe[0]);
	posix_spawn_file_actions_addclose(&action, cerr_pipe[0]);
	posix_spawn_file_actions_adddup2(&action, cout_pipe[1], 1);
	posix_spawn_file_actions_adddup2(&action, cerr_pipe[1], 2);

	posix_spawn_file_actions_addclose(&action, cout_pipe[1]);
	posix_spawn_file_actions_addclose(&action, cerr_pipe[1]);

	std::string ping_addr = "8.8.4.4";
	std::string command = ping_path + " -c 1 " + ping_addr;
	//std::string command = "/bin/ping -c 1 8.8.4.4";
	std::string argsmem[] = {"bash","-l","-c"}; // allows non-const access to literals
	char * args[] = {&argsmem[0][0],&argsmem[1][0],&argsmem[2][0],&command[0],nullptr};

	//std::cout << "Ping command: " << command << std::endl;

	pid_t pid;
	if(posix_spawnp(&pid, args[0], &action, NULL, &args[0], NULL) != 0)
	{
    	std::cout << "posix_spawnp failed with error: " << strerror(errno) << "\n";
    	exit(-2);
    }

    std::cout << "Ping process ID: " << pid << std::endl;

    close(cout_pipe[1]), close(cerr_pipe[1]); // close child-side of pipes

	bool& complete = t_info->complete;
	std::string& parent_buf = t_info->buf;

	std::string buf(512,' ');
	std::stringstream bufstream;

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
    }

	int bytes_read = read(cerr_pipe[0], &buf[0], buf.length());
	if (bytes_read > 0)
	{
		std::cout << "Got error message: " << buf.substr(0, bytes_read) << std::endl;
	}

    bytes_read = read(cout_pipe[0], &buf[0], buf.length());
	if (bytes_read > 0)
	{
		std::cout << "Read in " << bytes_read << " bytes from cout_pipe." << std::endl;
	}
	else
	{
		std::cout << "Read nothing from cout_pipe." << std::endl;
	}

	std::cout << "Done reading." << std::endl;

	parent_buf = buf.substr(0,bytes_read);

	std::cout << "Waiting for process to close." << std::endl;
	waitpid(pid,&exit_code,0);
	std::cout << "Child closed." << std::endl;

	posix_spawn_file_actions_destroy(&action);
    complete = true;

	std::cout << "Ping completed.  Main thread should now be taking over." << std::endl;

    pthread_exit(NULL);

    std::cout << "We should never get here." << std::endl;
}

int main()
{
    std::string buf;
    bool complete = false;

    struct thread_info t_info(complete, buf);

    pthread_t thread_id;
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);

    if (pthread_create(&thread_id, &attributes, &read_pipe, &t_info))
    {
    	std::cout << "failed to create thread" << std::endl;
    	return -3;
    }

    pthread_attr_destroy(&attributes);

	std::cout << "Joining child thread" << std::endl;

    if (pthread_join(thread_id, NULL) != 0)
	{
		std::cout << "Thread join error!" << std::endl;
	}

    std::cout << "Printing results from ping process!" << std::endl << std::endl;

    size_t time_pos = buf.rfind("time=");
    if (time_pos != std::string::npos)
    {
	    size_t result_pos = buf.find(" ms\n", time_pos);

	    std::string time_result = buf.substr(time_pos + 5, (result_pos - 1) - (time_pos + 5));

	    std::cout << time_result << std::endl;
	}
	else
	{
		std::cout << "No response." << std::endl;
	}

    /*
    for (unsigned int i = 0; i < buf.size(); i++)
    {
    	std::cout << buf[i];
    }
    std::cout << std::endl;
    */

    return 0;
}

