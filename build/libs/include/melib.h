#pragma once

#ifdef __cplusplus  
extern "C" {
#endif 

#include <time.h>
#include <stdbool.h>
#include <pspkerneltypes.h>
#include <psptypes.h>
#include <psprtc.h>

/**
* The mode of execution for a specific job.
*/
#define MELIB_EXEC_DEFAULT	0x0 /** Executes on the ME, unless Dynamic Rebalancing is turned on */
#define MELIB_EXEC_CPU		0x1 /**  Executes specifically on the main CPU, regardless of job mode. Always runs synchronously (gives a small delay between jobs).*/
#define MELIB_EXEC_ME		0x2 /** Executes specifically on the Media Engine, regardless of job mode. Always runs asynchronously.*/


/**
* Job Data is an integer pointer to an address with the data.
*/
typedef int JobData;
	
/**
* This typedef defines a JobFunction as an integer function with given data.
*/
typedef int (*JobFunction)(JobData ptr);



void J_Init(bool dynamicRebalance); /** Initialize the job manager with the option to dynamically rebalance loads. */

void J_EXECUTE_ME_ONCE(int (* fun)(int),int arg);
bool ME_JobDone();
int ME_JobReturnValue();
bool IsEmu();
void KILL_ME();

#ifdef __cplusplus  
}
#endif 