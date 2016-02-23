#ifndef COMMANDBUFFER_H
#define COMMANDBUFFER_H

#include "Marlin.h"

class CommandBuffer
{
  public:

    static void move2heatup();

    void processT0(bool bRetract);
    void processT1(bool bRetract);
    void processWipe();

#ifdef SDSUPPORT
    // constructor
    CommandBuffer () : t0(0), t1(0), wipe(0)  {}
    // destructor
    ~CommandBuffer ();

    uint8_t initScripts();
  private:
    // the structure of a single node
    struct t_cmdline{
	  char *str;
	  struct t_cmdline *next;
	};

    // command scripts for extruder change
    struct t_cmdline *t0;
    struct t_cmdline *t1;
    struct t_cmdline *wipe;

  private:
    void deleteScript(struct t_cmdline *script);
    uint8_t processScript(struct t_cmdline *script);
    struct t_cmdline* createScript();
    struct t_cmdline* readScript(const char *filename);
#else
    // constructor
    CommandBuffer() {}
    // destructor
    ~CommandBuffer() {}

    uint8_t initScripts() {}
#endif // SDSUPPORT
};

extern CommandBuffer cmdBuffer;
#endif
