Branch Predictor Simulator

In this project I made a branch predictor simulator to simulate three different predictors - bimodal, gshare, hybrid. I also implemented a BTB table for the predictors. The included traces were used to run the simulator and check against the validation runs. The report was only made to satisfy the grading requirements and as such does not go into detail regarding the simulator itself.

To run the simulator, type 'make' in console to run the Makefile and compile the code. Once that's done, give command line inputs as follows:

To simulate a bimodal predictor: sim bimodal <M2> <BTB size> <BTB assoc>
<tracefile>, where M2 is the number of PC bits used to index the bimodal table; BTB assoc
is the associativity of the BTB, BTB size and BTB assoc are 0 if no BTB is modeled.

To simulate a gshare predictor: sim gshare <M1> <N> <BTB size> <BTB assoc>
<tracefile>, where M1 and N are the number of PC bits and global branch history register
bits used to index the gshare table, respectively; BTB assoc is the associativity of the BTB, BTB
size and BTB assoc are 0 if no BTB is modeled.

To simulate a hybrid predictor: sim hybrid <K> <M1> <N> <M2> <BTB size>
<BTB assoc> <tracefile>, where K is the number of PC bits used to index the chooser
table, M1 and N are the number of PC bits and global branch history register bits used to index
the gshare table (respectively), and M2 is the number of PC bits used to index the bimodal table.
BTB assoc is the associativity of the BTB, BTB size and BTB assoc are 0 if no BTB is modeled.

(<tracefile> is the filename of the input trace.) 

The simulator outputs everything to console.