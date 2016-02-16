#ifndef COMMANDBUFFER_H
#define COMMANDBUFFER_H

#include "Marlin.h"

class CommandBuffer {
  public:
    // constructor
    CommandBuffer () : t0(0), t1(0), wipe(0)  {}

    // destructor
    ~CommandBuffer ();

    uint8_t initScripts();
    void processT0();
    void processT1();
    void processWipe();
//    FORCE_INLINE bool hasScriptT0() { return t0; }
//    FORCE_INLINE bool hasScriptT1() { return t1; }
//    FORCE_INLINE bool hasScriptWipe() { return wipe; }

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
};

extern CommandBuffer cmdBuffer;
#endif
