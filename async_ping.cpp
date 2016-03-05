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

	posix_spawn_file_actions_init(&action);
	posix_spawn_file_actions_addclose(&action, cout_pipe[0]);
	posix_spawn_file_actions_addclose(&action, cerr_pipe[0]);
	posix_spawn_file_actions_adddup2(&action, cout_pipe[1], 1);
	posix_spawn_file_actions_adddup2(&action, cerr_pipe[1], 2);

	posix_spawn_file_actions_addclose(&action, cout_pipe[1]);
	posix_spawn_file_actions_addclose(&action, cerr_pipe[1]);

	//char command[] = "/sbin/ping -c 1 8.8.4.4";
	std::string argsmem[] = {"/sbin/ping", "-c", "1", "8.8.4.4"};
	char* args[] = {&argsmem[0][0], &argsmem[1][0], &argsmem[2][0], &argsmem[3][0], nullptr};

	pid_t pid;
	if(posix_spawnp(&pid, args[0], &action, NULL, &args[0], NULL) != 0)
	{
    	std::cout << "posix_spawnp failed with error: " << strerror(errno) << "\n";
    	exit(-2);
    }

    //std::cout << "Ping process ID: " << pid << std::endl;

    // close child-side of pipes
    close(cout_pipe[1]);
    close(cerr_pipe[1]);

    struct timespec sleep_time;
    sleep_time.tv_nsec = 16000000;

	bool& complete = t_info->complete;
	std::string& parent_buf = t_info->buf;

	std::string buf(1024,' ');
	std::stringstream bufstream;

	std::vector<pollfd> plist = { {cout_pipe[0],POLLIN}, {cerr_pipe[0],POLLIN} };
	for ( int rval; (rval=poll(&plist[0],plist.size(),/*timeout*/5000))>0; ) {
    	if ( plist[0].revents&POLLIN) {
    		size_t bytes_read = read(cout_pipe[0], &buf[0], buf.length());
    		if (bytes_read > 0)
    		{
    			//std::cout << "Read in " << bytes_read << " bytes from cout_pipe." << std::endl;
    			bufstream << buf.substr(0,bytes_read);
    		}
    	}
    	else if ( plist[1].revents&POLLIN ) {
    		size_t bytes_read = read(cerr_pipe[0], &buf[0], buf.length());
    		if (bytes_read > 0)
    		{
    			//std::cout << "Read in " << bytes_read << " bytes from cerr_pipe." << std::endl;
    			std::cout << buf.substr(0,bytes_read);
    			bufstream << buf.substr(0,bytes_read);
    		}
    	}
    	else
    	{
			break; // nothing left to read
		}
    	if (waitpid(pid,&exit_code,WNOHANG) == 0)
    	{
    		break;
    	}
    	nanosleep(&sleep_time, NULL);
	}

	//std::cout << "Done reading." << std::endl;

	parent_buf = bufstream.str();

	posix_spawn_file_actions_destroy(&action);
    complete = true;

    return NULL;
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

    struct timespec sleep_time;
    sleep_time.tv_nsec = 16000000;

    while (!complete)
    {
    	//std::cout << "Sleeping for 16 ms..." << std::endl;
    	nanosleep(&sleep_time, NULL);
    }

    //std::cout << "Printing results from ping process!" << std::endl << std::endl;

    size_t time_pos = buf.rfind("time=");
    size_t result_pos = buf.find("---");

    std::string time_result = buf.substr(time_pos + 5, (result_pos - 5) - (time_pos + 5));

    std::cout << time_result << std::endl;


    /*
    for (unsigned int i = 0; i < buf.size(); i++)
    {
    	std::cout << buf[i];
    }
    std::cout << std::endl;
    */

    pthread_join(thread_id, NULL);

    return 0;
}

