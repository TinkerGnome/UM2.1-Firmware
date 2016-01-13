#ifndef COMMANDBUFFER_H
#define COMMANDBUFFER_H

#include "Marlin.h"

class CommandBuffer {
  public:
    // constructor
    CommandBuffer () : t0(0), t1(0)  {}

    // destructor
    ~CommandBuffer ();

    uint8_t initScripts();
    FORCE_INLINE uint8_t enqueT0() { return enqueScript(t0); }
    FORCE_INLINE uint8_t enqueT1() { return enqueScript(t1); }
    FORCE_INLINE bool hasScriptT0() { return t0; }
    FORCE_INLINE bool hasScriptT1() { return t1; }

  private:
    // the structure of a single node
    struct t_cmdline{
	  char *str;
	  struct t_cmdline *next;
	};

    // command scripts for extruder change
    struct t_cmdline *t0;
    struct t_cmdline *t1;

  private:
    void deleteScript(struct t_cmdline *script);
    uint8_t enqueScript(struct t_cmdline *script);
    struct t_cmdline* createScript();
    struct t_cmdline* readScript(const char *filename);
};

extern CommandBuffer cmdBuffer;
#endif
