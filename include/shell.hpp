#ifndef SHELL_HPP
#define SHELL_HPP

#include<limits.h>
#include<sys/types.h>

class Shell{
  private:

    char homeDirectory[PATH_MAX];
    //it will store the path of the directoyr form whcih the shell was started
    char previousDirectory[PATH_MAX];
    //this will be need when we do cd , cd .. ,etc,

    pid_t shellPid;
    //this will store the process id of the shell 
    bool running;
    //this controls the main loop , until it is true , it will keep executing the commands , once false we will come out of the shell

  public:

    Shell();

    bool initialize();
    void run();
    void stop();

    bool isRunning()const;

    const char* getHomeDirectory() const;
    const char* getPreviousDirectory() const;
    /*the const at the end means function won't modify the object , like in the function definition there will be no code which is modifying the object attributes , also this function cannot call another non const function of hte object, becasue that function might chagne the obejct , but it can call other const fucntino ,because they also can't modify the object */



    bool setPreviousDirectory(const char* path);
};
#endif

