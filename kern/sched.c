#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>


// Choose a user environment to run and run it.

#ifndef SCHED_PRIORITY
void
sched_yield(void)
{
	// Implement simple round-robin scheduling.
	// Search through 'envs' for a runnable environment,
	// in circular fashion starting after the previously running env,
	// and switch to the first such environment found.
	// It's OK to choose the previously running env if no other env
	// is runnable.
	// But never choose envs[0], the idle environment,
	// unless NOTHING else is runnable.

	// LAB 4: Your code here.
        //First run through the env_id from cur_env until the end
        //If a runnable task is found , schedule it .
        //otherwise check if there is any task from 1 to curenv which 
        //can be run 

        //cprintf("RR Schedule Yield is called\n");

        uint32_t index=0;
        envid_t cur_env_id = 1;
        //Find out , if any task is already running 
        if( curenv == NULL )
        {
             //cprintf("This is the First Task being scheduled\n");
             index = 1;
        }
        else
        {
             //we have already scheduled some task(s) on this system
             //Pick the next task in the list and schedule it 
            
             cur_env_id = curenv->env_id;

             //start searching from next env
             index = ENVX(cur_env_id) + 1;
        }

        //Run through the loop until end and see , if we can get a task to run
        for( ;index<NENV;index++)
        {
            if(envs[index].env_status == ENV_RUNNABLE)
            {
                //cprintf("ManiX- Scheduling task at %d\n", index);
                //cprintf("ManiX- Scheduling task at %08x\n", envs[index].env_id);
                env_run(&envs[index]);
                panic("Unable to run task at index %d\n",index);
            }
        }

        //if we have come until here, that means we didnot get a task until
        //end of list 
        //cprintf("Did not find any task at the right of the list- scan from left\n");
        for(index =1 ;index <= ENVX(cur_env_id);index++)
        {
            if(envs[index].env_status == ENV_RUNNABLE)
            {
                //cprintf("ManiX- Scheduling task at %08x\n", envs[index].env_id);
                env_run(&envs[index]);
                panic("Unable to run task at index %d\n",index);
            }
        }
        
         //cprintf("There is NO Task apart from Idle task , run it \n");
	// Run the special idle environment when nothing else is runnable.
	if (envs[0].env_status == ENV_RUNNABLE)
		env_run(&envs[0]);
	else {
		cprintf("Destroyed all environments - nothing more to do!\n");
		while (1)
			monitor(NULL);
	}
}

#else

void
sched_yield(void)
{
	// Implement simple round-robin scheduling.
	// Search through 'envs' for a runnable environment,
	// in circular fashion starting after the previously running env,
	// and switch to the first such environment found.
	// It's OK to choose the previously running env if no other env
	// is runnable.
	// But never choose envs[0], the idle environment,
	// unless NOTHING else is runnable.

	// LAB 4: Your code here.
        //First run through the env_id from cur_env until the end
        //If a runnable task is found , schedule it .
        //otherwise check if there is any task from 1 to curenv which 
        //can be run 

        //cprintf("Priority Schedule Yield is called\n");

        uint32_t index=0;
        int current_priority = 0 ;
        envid_t cur_env_id = 1;
        int task_to_schedule = 0;

        //Find out , if any task is already running 
        if( curenv == NULL )
        {
             //cprintf("This is the First Task being scheduled\n");
             current_priority = 0 ;
             index = 1 ;
        }
        else
        {
             //we have already scheduled some task(s) on this system
             //Pick the next task in the list and schedule it 
            
             cur_env_id = curenv->env_id;
             current_priority = curenv->priority;

             //start searching from next env
             index = ENVX(cur_env_id) + 1;
        }

        //Run through the loop until end and see , if we can get a task to run
        for( ;index<NENV;index++)
        {
            if( (envs[index].env_status == ENV_RUNNABLE) &&
                ( envs[index].priority >= current_priority)
              )
            {
                current_priority =  envs[index].priority ;
                task_to_schedule = index;
            }
        }

        if(task_to_schedule > 0 ) 
        {
             //found a task, schedule it 
             //cprintf("====>@right : Schedule task at %d\n",task_to_schedule);
             env_run(&envs[task_to_schedule]);
        }

        //if we have come until here, that means we didnot get a task until
        //end of list 
        //cprintf("Did not find any task at the right of the list- scan from left\n");
        for(index =1 ;index < ENVX(cur_env_id);index++)
        {
            if((envs[index].env_status == ENV_RUNNABLE) &&
              ( envs[index].priority >= current_priority)
              )
            {
                //cprintf("ManiX- Scheduling task at %d\n", index);
                current_priority =  envs[index].priority ;
                task_to_schedule = index;
                //panic("Unable to run task at index %d\n",index);
            }
        }
        
        if(task_to_schedule > 0 ) 
        {
             //found a task, schedule it 
             //cprintf("====>@Left :Schedule task at %d\n",task_to_schedule);
             env_run(&envs[task_to_schedule]);
        }
        else
        {
            //No task to right of me or left of me, >= my priority
            //So schedule myself
            if( envs[ENVX(cur_env_id)].env_status == ENV_RUNNABLE )
            {
                env_run(&envs[ENVX(cur_env_id)]);
            }
        }

         cprintf("There is NO Task apart from Idle task , run it \n");
	// Run the special idle environment when nothing else is runnable.
	if (envs[0].env_status == ENV_RUNNABLE)
		env_run(&envs[0]);
	else {
		cprintf("Destroyed all environments - nothing more to do!\n");
		while (1)
			monitor(NULL);
	}
}
#endif
