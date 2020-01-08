#include <string>

#include <iostream>

#include <math.h>

#include <fstream>

#include <sstream>

#include <bitset>

#include <math.h>

#include <cstring>

#include <sstream>

#include <stdio.h>

#include <stdlib.h>

#include <iomanip>


using namespace std;

//global counters
int predictions = 0, bimodal_mispredictions = 0, gshare_mispredictions = 0, hybrid_mispredictions = 0, BTB_hits = 0, BTB_misses = 0, BTB_miss_but_taken =0;

//class table content template (index, counter)
class table {
  public:
    int index;
  int counter;

};

//class branch hisotry register template
class reg {

  public:
    int value;
};

//class BTB template
class BTB_template {

public:
int tag;
int index;
int LRU;
int valid;
};

//instantiate tables (bimodal, gshare, hybrid) and branch history register
table bimodal[65536];
table gshare[65536];
table hybrid[8192];
reg global_reg;
BTB_template BTB[8192][8192];

//bimodal index calculator
int bimodal_index_calc(int M2, unsigned int address) {
  int a, mask, index;

  a = address >> 2;
  mask = 16777215;

  mask = mask >> (24 - M2);

  index = mask & a;

  return index;

}

//bimodal predictor
int bimodal_predictor(char n, int bimodal_index) {
  int prediction;
 // int actual;
  if (bimodal[bimodal_index].counter >= 2) {
    prediction = 1;
  }

  if (bimodal[bimodal_index].counter < 2) {
    prediction = 0;
  }

  return prediction;
}

//bimodal counter updater
void bimodal_counter_update(int prediction, char n, int bimodal_index) {
  int actual=0;
  if (n == 't') {
    actual = 1;
  } else if (n == 'n') {
    actual = 0;
  }

  if (actual == 1 && bimodal[bimodal_index].counter < 3) {
    bimodal[bimodal_index].counter++;
  }

  if (actual == 0 && bimodal[bimodal_index].counter > 0) {
    bimodal[bimodal_index].counter--;
  }

  if (actual != prediction) {
    bimodal_mispredictions++;
    hybrid_mispredictions++;
  }

}

//gshare index calculator
int gshare_index_calc(int M1, int N, unsigned int address, int reg_val) {

  unsigned int top, bottom, m_bits, index, a,  mask, mask1, intermediate_address, final_address;
  a = address >> 2;
  mask = 16777215;
  mask1 = 16777215;

  mask = mask >> (24 - M1);
  m_bits = mask & a; // have M+1 to 2 bits of 24 bit binary PC

  mask = mask >> N;

  bottom = mask & m_bits;

  m_bits = m_bits >> (M1 - N);

  mask1 = mask1 >> (24 - N);

  top = mask1 & m_bits;

  intermediate_address = top ^ reg_val;

  intermediate_address = intermediate_address << (M1 - N);

  final_address = intermediate_address | bottom;

  index = final_address;
  //cout << "gshare index: " << index << endl;
  return index;

}

//gshare predictor and branch history updater
int gshare_predictor(char n, int gshare_index, int N) {

  int prediction=0;
  int actual=0;

  //cout << "index: " << gshare_index << endl;
  //cout << "old val: " << gshare[gshare_index].counter << endl;

  if (gshare[gshare_index].counter >= 2) {
    prediction = 1;
  }

  if (gshare[gshare_index].counter < 2) {
    prediction = 0;
  }

  if (n == 't') {
    actual = 1;
  } else if (n == 'n') {
    actual = 0;
  }

  //global reg update
  int reg_valueish = global_reg.value >> 1;
  int actualish = actual << (N - 1);
  //cout << "global reg value: " << global_reg.value << endl;
  global_reg.value = reg_valueish | actualish;
  //cout << "BRH updated: " << global_reg.value << endl;

  return prediction;

}

//gshare counter updater
void gshare_counter_update(int prediction, char n, int gshare_index) {

  int actual=0;
  if (n == 't') {
    actual = 1;
  } else if (n == 'n') {
    actual = 0;
  }

  if (actual == 1 && gshare[gshare_index].counter < 3) {
    gshare[gshare_index].counter++;
  }

  if (actual == 0 && gshare[gshare_index].counter > 0) {
    gshare[gshare_index].counter--;
  }
  //cout << "new val: " << gshare[gshare_index].counter << endl;
  if (actual != prediction) {
    gshare_mispredictions++;
    hybrid_mispredictions++;
  }

}

//chooser index calculator
int hybrid_chooser_index_calc(int k, int address) {

  int a, mask, index;

  a = address >> 2;
  mask = 16777215;

  mask = mask >> (24 - k);

  index = mask & a;

  return index;

}
void predictor(unsigned int address,
  const char * c, string type, int M1, int M2, int N, int k) {

  int chooser_index = 0;
  int bimodal_index = 0;
  int gshare_index = 0;
  int bimodal_prediction;
  int gshare_prediction;

  //start bimodal
  if (type == "bimodal") {

    bimodal_index = bimodal_index_calc(M2, address);
    bimodal_prediction = bimodal_predictor( * c, bimodal_index);
    bimodal_counter_update(bimodal_prediction, * c, bimodal_index);

  }
  //end bimodal

  //start gshare
  if (type == "gshare") {

    //cout << "PC: "<< address << " "<< *c<<endl;

    gshare_index = gshare_index_calc(M1, N, address, global_reg.value);
    gshare_prediction = gshare_predictor( * c, gshare_index, N);
    gshare_counter_update(gshare_prediction, * c, gshare_index);
  }
  //end gshare

  //start hybrid
  if (type == "hybrid") {
    int actual=0;
//int final_prediction;
    bimodal_index = bimodal_index_calc(M2, address);
    bimodal_prediction = bimodal_predictor( * c, bimodal_index);
    gshare_index = gshare_index_calc(M1, N, address, global_reg.value);
    gshare_prediction = gshare_predictor( * c, gshare_index, N);
    chooser_index = hybrid_chooser_index_calc(k, address);

    int picker = hybrid[chooser_index].counter;

    if (picker >= 2) {
      //final_prediction = gshare_prediction;
      gshare_counter_update(gshare_prediction, * c, gshare_index);
    } else {
      //final_prediction = bimodal_prediction;
      bimodal_counter_update(bimodal_prediction, * c, bimodal_index);
    }

    if ( * c == 't') {
      actual = 1;
    } else if ( * c == 'n') {
      actual = 0;
    }

    if (actual == gshare_prediction && actual == bimodal_prediction) {} 

    else if (actual == gshare_prediction && actual != bimodal_prediction) {

    	  if (hybrid[chooser_index].counter < 3) {
    	    hybrid[chooser_index].counter++;
    	  }

    } 
    
    else if (actual != gshare_prediction && actual == bimodal_prediction) {

    	  if (hybrid[chooser_index].counter > 0) {
    	    hybrid[chooser_index].counter--;
    	  }

    } 

    else if (actual != gshare_prediction && actual != bimodal_prediction) {}

  } 
  //end hybrid

} 
//end predictor

//BTB index calculator
int BTB_index_calc(int address, int tag_bits){

unsigned int mask = 16777215;

int index;

unsigned int a = address >> 2;



mask = mask >> (tag_bits+2);

index = mask & a; 

return index;

}

//BTB tag calculator

int BTB_tag_calc(int address, int index_bits){
unsigned int mask = 16777215;

int tag;

unsigned int a = address >> 2;



a = a>>index_bits;
mask = mask >> (2+index_bits);

tag = mask & a; 

return tag;



}

void BTB_LRU_update(int set, int block_num, int assoc) { //LRU counter updating

   for (int j = 0; j < assoc; j++) {
      if (BTB[set][j].LRU < BTB[set][block_num].LRU) {

         BTB[set][j].LRU += 1;
      }
   }

   //resetting LRU of hit block

   BTB[set][block_num].LRU = 0;

} //end LRU update

int BTB_find_LRU(int index, int BTB_assoc) { //finding LRU block to evict

   int LRUblock = 0;
   for (int i = 0; i < BTB_assoc; i++) {
      int ctr = 0;
      for (int j = 0; j < BTB_assoc; j++) {

         if (BTB[index][i].LRU >= BTB[index][j].LRU) {
            ctr++;
         }
      }

      if (ctr == BTB_assoc) {
         LRUblock = i;
      }
   }

   return LRUblock;
} //extracting LRU


//BTB

void BTB_check(unsigned int address,
  const char * c, string type, int M1, int M2, int N, int k, int BTB_assoc, int BTB_size){

int BTB_sets = BTB_size/(4*BTB_assoc);

int index_bits = log2(BTB_sets);

//int offset_bits = 2;
int tag_bits = 24-2-index_bits;
int BTB_index = BTB_index_calc(address, tag_bits);
int BTB_tag = BTB_tag_calc(address,  index_bits);
 int actual=0;//,prediction;
  if (*c == 't') {
    actual = 1;
  } else if (*c == 'n') {
    actual = 0;
  }
//cout<<"BTB tag"<<BTB_tag<<endl;
//cout<<"BTB index"<<BTB_index<<endl;

int BTB_hit=0;

for(int i = 0; i < BTB_assoc; i++){

if(BTB[BTB_index][i].tag==BTB_tag){
BTB_hit = 1;
BTB_LRU_update(BTB_index, i, BTB_assoc);

break;
}

}

if(BTB_hit == 1){
BTB_hits ++;
predictor(address, c, type, M1, M2, N, k);
//cout<<"BTB HIT"<<endl;
}

else if(BTB_hit == 0){

BTB_misses++;
//prediction = 0;
if(actual == 1){BTB_miss_but_taken++;}
int invalid_found = 0, invalid_found_at;

for(int j =0; j< BTB_assoc; j++){

if(BTB[BTB_index][j].valid == 0){invalid_found = 1; invalid_found_at = j; break;}
else {continue;}

}

if(invalid_found == 1){

BTB[BTB_index][invalid_found_at].tag = BTB_tag;
BTB[BTB_index][invalid_found_at].valid = 1;
BTB_LRU_update(BTB_index, invalid_found_at, BTB_assoc);

}

else if(invalid_found == 0){

int LRU_block = BTB_find_LRU(BTB_index, BTB_assoc);
BTB[BTB_index][LRU_block].tag = BTB_tag;
BTB[BTB_index][LRU_block].valid = 1;
BTB_LRU_update(BTB_index, LRU_block, BTB_assoc);

}

}
}


//main predictor

int main(int argc, char * argv[]) {
  //inputs

  string type;
  int M1 = 0;
  int M2 = 0;
  int k = 0;
  int N = 0;
  int BTB_size = 0;
  int BTB_assoc = 0;
  string trace_file;

//assigning variables based on number of arguments
  if (argc == 6) {//bimodal
    type = argv[1];
    M2 = atoi(argv[2]);
    BTB_size = atoi(argv[3]);
    BTB_assoc = atoi(argv[4]);
    trace_file = argv[5];

  } 

  else if (argc == 7) {//gshare

    type = argv[1];
    M1 = atoi(argv[2]);
    N = atoi(argv[3]);
    BTB_size = atoi(argv[4]);
    BTB_assoc = atoi(argv[5]);
    trace_file = argv[6];
  }

  else if (argc == 9) {//hybrid

    type = argv[1];
    k = atoi(argv[2]);
    M1 = atoi(argv[3]);
    N = atoi(argv[4]);
    M2 = atoi(argv[5]);
    BTB_size = atoi(argv[6]);
    BTB_assoc = atoi(argv[7]);
    trace_file = argv[8];
  }

//variable declarations for index and rows
  int bimodal_index_bits=0;
  int bimodal_rows=0;

  int gshare_index_bits=0;
  int gshare_rows=0;
  //int global_reg_bits;
  //int global_reg_rows;

  int hybrid_chooser_bits=0;
  int hybrid_chooser_rows=0;
  //unsigned int global_reg=0;


if(BTB_size !=0 && BTB_assoc !=0){
int BTB_sets = BTB_size/(4*BTB_assoc);
for(int i=0; i<BTB_sets; i++){
for(int j=0; j<BTB_assoc; j++){

BTB[i][j].tag = 0;
BTB[i][j].valid = 0;
BTB[i][j].index = 0;
BTB[i][j].LRU = j;

}

}

}

  if (type == "bimodal") {//initializing bimodal table

    bimodal_index_bits = M2;
    bimodal_rows = pow(2, bimodal_index_bits);

    for (int i = 0; i < bimodal_rows; i++) {

      bimodal[i].index = i;
      bimodal[i].counter = 2;

    }

  }

  if (type == "gshare") {//initializing gshare table
    gshare_index_bits = M1;
    //global_reg_bits = N;
    gshare_rows = pow(2, gshare_index_bits);
    //global_reg_rows = pow(2, global_reg_bits);

    for (int i = 0; i < gshare_rows; i++) {
      gshare[i].index = i;
      gshare[i].counter = 2;

    }

    //global_reg.value = 0;

  }

  if (type == "hybrid") {//initializing bimodal, gshare, and hybrid tables

    bimodal_index_bits = M2;
    bimodal_rows = pow(2, bimodal_index_bits);
    gshare_index_bits = M1;
    //global_reg_bits = N;
    gshare_rows = pow(2, gshare_index_bits);
   // global_reg_rows = pow(2, global_reg_bits);
    hybrid_chooser_bits = k;
    hybrid_chooser_rows = pow(2, hybrid_chooser_bits);

    for (int i = 0; i < bimodal_rows; i++) {

      bimodal[i].index = i;
      bimodal[i].counter = 2;

    }

    for (int i = 0; i < gshare_rows; i++) {
      gshare[i].index = i;
      gshare[i].counter = 2;

    }

   // global_reg.value = 0;

    for (int i = 0; i < hybrid_chooser_rows; i++) {

      hybrid[i].index = i;
      hybrid[i].counter = 1;

    }
  }

  //global_reg.value =0;

  ifstream infile;
  string line;

  string sub1, sub2;
  unsigned int address;
  infile.open(trace_file.c_str());
  while (getline(infile, line)) {

    istringstream s(line);

    s >> sub1;
    s >> sub2;

    istringstream buffer(sub1);

    buffer >> hex >> address;
    //cout<< address << endl;
predictions++;
	if(BTB_size == 0 && BTB_assoc == 0){
    predictor(address, sub2.c_str(), type, M1, M2, N, k);
}

else {BTB_check(address, sub2.c_str(), type, M1, M2, N, k, BTB_assoc, BTB_size);}


  };
  infile.close();


  // print stats
  cout << "COMMAND" << endl;
  for (int i = 0; i < argc; i++) {

    cout << argv[i] << " ";

  }

  cout << endl;

unsigned int temptag;
   int tempLRU;
   bool tempvalid;
int BTB_sets=0;
if(BTB_size!=0){
int BTB_sets = BTB_size/(4*BTB_assoc);


   for (int ind = 0; ind < BTB_sets; ind++) { //BTB sort
      for (int i = 0; i < BTB_assoc; ++i)

      {

         for (int j = i + 1; j < BTB_assoc; ++j)

         {

            if (BTB[ind][i].LRU > BTB[ind][j].LRU)

            {

               temptag = BTB[ind][j].tag; //repeat for others

               BTB[ind][j].tag = BTB[ind][i].tag;

               BTB[ind][i].tag = temptag;

               tempLRU = BTB[ind][j].LRU; //repeat for others

               BTB[ind][j].LRU = BTB[ind][i].LRU;

               BTB[ind][i].LRU = tempLRU;

               tempvalid = BTB[ind][j].valid; //repeat for others

               BTB[ind][j].valid = BTB[ind][i].valid;

               BTB[ind][i].valid = tempvalid;

            }

         }

      }

   }
}
if(BTB_size !=0 && BTB_assoc !=0){
  cout << "OUTPUT" << endl;
cout<<"size of BTB:	"<<BTB_size<<endl;
  cout << " number of branches:		" << predictions << endl;
cout<<"number of predictions from branch predictor:	"<<BTB_hits << endl;
int final_mispreds=0;
if(type=="bimodal"){final_mispreds = bimodal_mispredictions;}
if(type=="gshare"){final_mispreds = gshare_mispredictions;}
if(type=="hybrid"){final_mispreds = hybrid_mispredictions;}
cout<<"number of mispredictions from branch predictor:	"<<final_mispreds<<endl;
cout << "number of branches miss in BTB and taken:	"<<BTB_miss_but_taken<<endl;
int tot_mispreds = final_mispreds+BTB_miss_but_taken;
cout << "total mispredictions:	"<<tot_mispreds<<endl;
float mispred_rate = (float)(tot_mispreds)/(predictions);
mispred_rate = mispred_rate*100;
    cout.precision(2);
    cout.unsetf(ios::floatfield);
    cout.setf(ios::fixed, ios::floatfield);
cout<<"misprediction rate:	"<<mispred_rate<<"%"<<endl;
cout.unsetf(ios::floatfield);

cout<<"FINAL BTB CONTENTS"<<endl;
BTB_sets = BTB_size/(4*BTB_assoc);
for(int i =0; i< BTB_sets; i++){

cout<<"set   "<<dec<<i<<":";

for(int j=0; j<BTB_assoc;j++){
if(BTB[i][j].tag!=0){
cout<<"   "<<hex<<BTB[i][j].tag;
}
else if(BTB[i][j].tag==0){
cout<<"   "<<" ";

}
}
cout<<endl;
}

cout<<endl;
}

else{
  cout << "OUTPUT" << endl;
  cout << " number of predictions: " << predictions << endl;
}

  if (type == "bimodal"&&BTB_size == 0) {

    cout << " number of mispredictions: " << bimodal_mispredictions << endl;

    float miss_rate;

    miss_rate = (float)(bimodal_mispredictions * 100) / (predictions);
    cout.precision(2);
    cout.unsetf(ios::floatfield);
    cout.setf(ios::fixed, ios::floatfield);
    cout << " misprediction rate: " << miss_rate << "%" << endl;
    cout.unsetf(ios::floatfield);
    cout << "FINAL BIMODAL CONTENTS" << endl;

    for (int i = 0; i < bimodal_rows; i++) {

      cout <<dec<< bimodal[i].index << " " <<dec<< bimodal[i].counter << endl;

    }
  }
  if (type == "bimodal"&&BTB_size != 0) {

    cout << "FINAL BIMODAL CONTENTS" << endl;

    for (int i = 0; i < bimodal_rows; i++) {

      cout << dec<<bimodal[i].index << " " <<dec<< bimodal[i].counter << endl;

    }
  }

  if (type == "gshare"&&BTB_size == 0) {

    cout << " number of mispredictions: " << gshare_mispredictions << endl;

    float miss_rate;

    miss_rate = (float)(gshare_mispredictions * 100) / (predictions);
    cout.precision(2);
    cout.unsetf(ios::floatfield);
    cout.setf(ios::fixed, ios::floatfield);
    cout << " misprediction rate: " << miss_rate << "%" << endl;
    cout.unsetf(ios::floatfield);

    cout << "FINAL GSHARE CONTENTS" << endl;

    for (int i = 0; i < gshare_rows; i++) {

      cout <<dec<< gshare[i].index << " " <<dec<< gshare[i].counter << endl;

    }
  }

  if (type == "gshare"&&BTB_size != 0) {


    cout << "FINAL GSHARE CONTENTS" << endl;

    for (int i = 0; i < gshare_rows; i++) {

      cout << dec<<gshare[i].index << " " <<dec<< gshare[i].counter << endl;

    }
  }
  if (type == "hybrid"&&BTB_size == 0) {

    cout << " number of mispredictions: " << hybrid_mispredictions << endl;

    float miss_rate;

    miss_rate = (float)(hybrid_mispredictions * 100) / (predictions);
    cout.precision(2);
    cout.unsetf(ios::floatfield);
    cout.setf(ios::fixed, ios::floatfield);
    cout << " misprediction rate: " << miss_rate << "%" << endl;
    cout.unsetf(ios::floatfield);
    cout << "FINAL CHOOSER CONTENTS" << endl;

    for (int i = 0; i < hybrid_chooser_rows; i++) {

      cout <<dec<< hybrid[i].index << " " <<dec<< hybrid[i].counter << endl;

    }
    cout << "FINAL GSHARE CONTENTS" << endl;

    for (int i = 0; i < gshare_rows; i++) {

      cout <<dec<< gshare[i].index << " " <<dec<< gshare[i].counter << endl;

    }

    cout << "FINAL BIMODAL CONTENTS" << endl;

    for (int i = 0; i < bimodal_rows; i++) {

      cout <<dec<< bimodal[i].index << " " <<dec<< bimodal[i].counter << endl;

    }

  }

  if (type == "hybrid"&&BTB_size != 0) {

    cout << "FINAL CHOOSER CONTENTS" << endl;

    for (int i = 0; i < hybrid_chooser_rows; i++) {

      cout <<dec<< hybrid[i].index << " " <<dec<< hybrid[i].counter << endl;

    }
    cout <<dec<< "FINAL GSHARE CONTENTS" <<dec<< endl;

    for (int i = 0; i < gshare_rows; i++) {

      cout << dec<<gshare[i].index << " " <<dec<< gshare[i].counter << endl;

    }

    cout << "FINAL BIMODAL CONTENTS" << endl;

    for (int i = 0; i < bimodal_rows; i++) {

      cout <<dec<< bimodal[i].index << " " <<dec<< bimodal[i].counter << endl;

    }

  }



  return 0;
};
