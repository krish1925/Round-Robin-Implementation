#include <fcntl.h>
#include <stdbool.h>
#include <stdckdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <unistd.h>

/* A process table entry.  */
struct process
{
  long pid;
  long arrival_time;
  long burst_time;
  long remaining_time;
  bool process_started;
  bool process_arrived;
  bool process_completed;

  TAILQ_ENTRY (process) pointers;

  /* Additional fields here */
  /* End of "Additional fields here" */
};

TAILQ_HEAD (process_list, process);

/* Skip past initial nondigits in *DATA, then scan an unsigned decimal
   integer and return its value.  Do not scan past DATA_END.  Return
   the integer’s value.  Report an error and exit if no integer is
   found, or if the integer overflows.  */
static long
next_int (char const **data, char const *data_end)
{
  long current = 0;
  bool int_start = false;
  char const *d;

  for (d = *data; d < data_end; d++)
    {
      char c = *d;
      if ('0' <= c && c <= '9')
	{
	  int_start = true;
	  if (ckd_mul (&current, current, 10)
	      || ckd_add (&current, current, c - '0'))
	    {
	      fprintf (stderr, "integer overflow\n");
	      exit (1);
	    }
	}
      else if (int_start)
	break;
    }

  if (!int_start)
    {
      fprintf (stderr, "missing integer\n");
      exit (1);
    }

  *data = d;
  return current;
}

/* Return the first unsigned decimal integer scanned from DATA.
   Report an error and exit if no integer is found, or if it overflows.  */
static long
next_int_from_c_str (char const *data)
{
  return next_int (&data, strchr (data, 0));
}

/* A vector of processes of length NPROCESSES; the vector consists of
   PROCESS[0], ..., PROCESS[NPROCESSES - 1].  */
struct process_set
{
  long nprocesses;
  struct process *process;
};

/* Return a vector of processes scanned from the file named FILENAME.
   Report an error and exit on failure.  */
static struct process_set
init_processes (char const *filename)
{
  int fd = open (filename, O_RDONLY);
  if (fd < 0)
    {
      perror ("open");
      exit (1);
    }

  struct stat st;
  if (fstat (fd, &st) < 0)
    {
      perror ("stat");
      exit (1);
    }

  size_t size;
  if (ckd_add (&size, st.st_size, 0))
    {
      fprintf (stderr, "%s: file size out of range\n", filename);
      exit (1);
    }

  char *data_start = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED)
    {
      perror ("mmap");
      exit (1);
    }

  char const *data_end = data_start + size;
  char const *data = data_start;

  long nprocesses = next_int (&data, data_end);
  if (nprocesses <= 0)
    {
      fprintf (stderr, "no processes\n");
      exit (1);
    }

  struct process *process = calloc (sizeof *process, nprocesses);
  if (!process)
    {
      perror ("calloc");
      exit (1);
    }

  for (long i = 0; i < nprocesses; i++)
    {
      process[i].pid = next_int (&data, data_end);
      process[i].arrival_time = next_int (&data, data_end);
      process[i].burst_time = next_int (&data, data_end);
      process[i].process_started = false;
      process[i].remaining_time = process[i].burst_time;
      process[i].process_arrived = false;
      process[i].process_completed = false;
      if (process[i].burst_time == 0)
	{
	  fprintf (stderr, "process %ld has zero burst time\n",
		   process[i].pid);
	  exit (1);
	}
    }

  if (munmap (data_start, size) < 0)
    {
      perror ("munmap");
      exit (1);
    }
  if (close (fd) < 0)
    {
      perror ("close");
      exit (1);
    }
  return (struct process_set) {nprocesses, process};
}
void enqueue_new_arrivals(long current_time, struct process_set *ps, struct process_list *list) {
  for(int i = 0; i < ps->nprocesses; i++) {
    if(ps->process[i].arrival_time <= current_time && !ps->process[i].process_arrived) {
      TAILQ_INSERT_TAIL(list, &ps->process[i], pointers);
      ps->process[i].process_arrived = true;
    }
  }
}
int
main (int argc, char *argv[])
{
  if (argc != 3)
    {
      fprintf (stderr, "%s: usage: %s file quantum\n", argv[0], argv[0]);
      return 1;
    }

  struct process_set ps = init_processes (argv[1]);
  long quantum_length = (strcmp (argv[2], "median") == 0 ? -1
			 : next_int_from_c_str (argv[2]));
  if (quantum_length == 0)
    {
      fprintf (stderr, "%s: zero quantum length\n", argv[0]);
      return 1;
    }

  struct process_list list;
  TAILQ_INIT (&list);
  long cswitch = 1;
  long total_wait_time = 0;
  long total_response_time = 0;
  long completed_processes = 0;
  long current_time = 0;
  long run_time = 0;
  //long context_switches = 0;
  //int counter = 0;
  bool is_median_picked = false;
  if(quantum_length == -1){
    is_median_picked = true;
  }

//int current_running_processes = 0;


while (ps.nprocesses != completed_processes) {
    // Check if new processes have arrived
    for (int i = 0; i < ps.nprocesses; i++) {
        if (ps.process[i].arrival_time <= current_time && !ps.process[i].process_arrived) {
            TAILQ_INSERT_TAIL(&list, &ps.process[i], pointers);
            ps.process[i].process_arrived = true;
            //current_running_processes++;
            //printf("New process %ld has arrived \n", ps.process[i].pid);
        }
    }
    if (is_median_picked) {
      long run_times[ps.nprocesses];
      long count = 0;
      // Collect run times of processes that have arrived but not yet completed
      for(int i = 0; i < ps.nprocesses; i++) {
        if(ps.process[i].process_arrived && !ps.process[i].process_completed) {
          run_times[count++] = ps.process[i].burst_time - ps.process[i].remaining_time;
        }
      }
      // Only proceed if we have at least one run time
      if (count > 0) {
        // Sort the collected run times
        for(int i = 0; i < count; i++) {
          for(int j = i + 1; j < count; j++) {
            if(run_times[i] > run_times[j]) {
              long temp = run_times[i];
              run_times[i] = run_times[j];
              run_times[j] = temp;
            }
          }
        }
        // printf("[");
        // for(int i = 0; i < count; i++) {
        //     printf(" %ld", run_times[i]);
        // }
        // printf(" ]\n");
        // Calculate the median quantum
        if (count % 2 == 0) {
          quantum_length = (run_times[count / 2 - 1] + run_times[count / 2]);
          if((quantum_length % 2 )== 0){
            quantum_length = quantum_length / 2;
          }
          else{
            quantum_length = quantum_length/2;
            if(quantum_length %2 != 2){
              quantum_length++;
            }
          }
        } else {
          quantum_length = run_times[count / 2];
        }
        if(quantum_length < 1){
          quantum_length = 1;
        }
      } else {
        quantum_length = 1; // Default to 1 if no process is in the queue
      }
    }

    if (!TAILQ_EMPTY(&list)) {
        struct process *current_process = TAILQ_FIRST(&list);

        // Record response time if it's the first run
        if (!current_process->process_started) {
            current_process->process_started = true;
            //printf("----->Response time inctreased from %ld by %ld     ------>\n", total_response_time, current_time - current_process->arrival_time);
            total_response_time += current_time - current_process->arrival_time;
            
        }
        // Run the current process
        run_time = (current_process->remaining_time >= quantum_length) ? quantum_length : current_process->remaining_time;
        //printf("Process PID = %ld has run time = %ld, starts at time %ld and ends at time %ld \n", current_process->pid, run_time, current_time, current_time + run_time);

        current_time += run_time; // Advance time by run_time

        // Check and insert any new arrivals during the quantum
        for (int k = 0; k < ps.nprocesses; k++) {
            if (ps.process[k].arrival_time <= current_time && !ps.process[k].process_arrived) {
                TAILQ_INSERT_TAIL(&list, &ps.process[k], pointers);
                ps.process[k].process_arrived = true;
                //printf("New process %ld has arrived during quantum at time %ld \n", ps.process[k].pid, ps.process[k].arrival_time);
            }
        }

        current_process->remaining_time -= run_time;

        if (current_process->remaining_time == 0) { // Process completion check
            completed_processes++;
            //printf("Process PID = %ld completed at time %ld.\n", current_process->pid, current_time);
            current_process->process_completed = true;
            //printf("-000000->wait time inctreased from %ld by %ld    00000--->\n", total_wait_time,current_time - current_process->arrival_time - current_process->burst_time);

            total_wait_time += current_time - current_process->arrival_time - current_process->burst_time;
            TAILQ_REMOVE(&list, current_process, pointers);
        }

        // Check for the next process to potentially switch to
        struct process *next_process = TAILQ_NEXT(current_process, pointers);

        // Perform a context switch if there is a next process and it's different from the current one
        if (next_process && current_process != next_process) {
            //printf("Context switch from PID %ld to PID %ld at time %ld.\n", current_process->pid, next_process->pid, current_time);
            current_time += cswitch; // Account for context switch time
            //context_switches++;
        }

        if (current_process->remaining_time > 0) {
            // Process did not complete; remove it, then re-queue
            TAILQ_REMOVE(&list, current_process, pointers);
            TAILQ_INSERT_TAIL(&list, current_process, pointers);
        }
    } else {
        current_time++; // If the list is empty, just move forward in time
    }
}


  printf ("Average wait time: %.2f\n",
	  total_wait_time / (double) ps.nprocesses);
  printf ("Average response time: %.2f\n",
	  total_response_time / (double) ps.nprocesses);

  if (fflush (stdout) < 0 || ferror (stdout))
    {
      perror ("stdout");
      return 1;
    }

  free (ps.process);
  return 0;
}
