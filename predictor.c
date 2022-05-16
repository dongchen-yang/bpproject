//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Dongchen Yang";
const char *studentID   = "A59003269";
const char *email       = "doy001@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

//define number of bits required for indexing the BHT here. 
int ghistoryBits = 12; // Number of bits used for Global History
int lhistoryBits = 10 ; //Number of bits used for local history
int pcIndexBits = 10; //Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;


//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
//TODO: Add your own Branch Predictor data structures here
//

//for the convinience of getting the lower bits
uint32_t global_mask;
uint32_t local_mask;
uint32_t pc_mask;


//gshare
uint8_t *bht_gshare;
uint64_t ghistory;

//tournament

uint32_t *lbht_tournament;
uint32_t *lpht_tournament;
uint32_t *gpht_tournament;
uint32_t *choice_pht;

//custom -- perceptron

//hypter parameters to tune for perceptron


#define PCSIZE 426
#define HEIGHT 20
#define SATUATELEN 8
#define MASK_PC(x) ((x * 19) % PCSIZE)

int16_t W[PCSIZE][HEIGHT + 1];
int16_t ghistory_percep[HEIGHT];
int32_t threshold;
uint8_t recent_prediction = NOTTAKEN;
uint8_t need_train = 0;


//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//

//gshare functions
void init_gshare() {
 int bht_entries = 1 << ghistoryBits;
  bht_gshare = (uint8_t*)malloc(bht_entries * sizeof(uint8_t));
  int i = 0;
  for(i = 0; i< bht_entries; i++){
    bht_gshare[i] = WN;
  }
  ghistory = 0;
}



uint8_t 
gshare_predict(uint32_t pc) {
  //get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits; 
  uint32_t pc_lower_bits = pc & (bht_entries-1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch(bht_gshare[index]){
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
      return NOTTAKEN;
  }
}

void
train_gshare(uint32_t pc, uint8_t outcome) {
  //get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries-1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  //Update state of entry in bht based on outcome
  switch(bht_gshare[index]){
    case WN:
      bht_gshare[index] = (outcome==TAKEN)?WT:SN;
      break;
    case SN:
      bht_gshare[index] = (outcome==TAKEN)?WN:SN;
      break;
    case WT:
      bht_gshare[index] = (outcome==TAKEN)?ST:WN;
      break;
    case ST:
      bht_gshare[index] = (outcome==TAKEN)?ST:WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
  }

  //Update history register
  ghistory = ((ghistory << 1) | outcome);  
}

void
cleanup_gshare() {
  free(bht_gshare);
}


uint32_t
make_mask(uint32_t size)
{
  uint32_t mmask = 0;
  for(int i=0;i<size;i++)
  {
    mmask = mmask | 1<<i;
  }
  return mmask;
}

//tournament predictor functions
void init_tournament() {
  //local branch history table
  
  ghistory = 0;
  global_mask = make_mask(ghistoryBits);
  local_mask = make_mask(lhistoryBits);
  pc_mask = make_mask(pcIndexBits);

  int lbht_entry = 1<<pcIndexBits;
  
  lbht_tournament = (uint32_t*)malloc(lbht_entry * sizeof(uint32_t));

  for(int i = 0; i< lbht_entry; i++){
    lbht_tournament[i] = SN;
  }
  //local pattern history table

  int lhistoryentry = 1 << lhistoryBits;
  lpht_tournament = (uint32_t*)malloc(lhistoryentry * sizeof(uint32_t));
  for (int i = 0; i< lhistoryentry; i++){
    lpht_tournament[i] = WN;
  }

  //global PHT

  int ghistoryentry = 1 <<ghistoryBits;
  gpht_tournament = (uint32_t*)malloc(ghistoryentry * sizeof(uint32_t));
  for(int i = 0; i < ghistoryentry; i++){
    gpht_tournament[i] = WN;
  }

  //Chooser
  
  choice_pht = (uint32_t*)malloc(ghistoryentry* sizeof(uint32_t));
  for (int i = 0; i<ghistoryentry; i++){
    choice_pht[i] = 2;
  }

}




uint8_t 
tournament_predict(uint32_t pc) {
  //get lower ghistoryBits of pc

  uint32_t prediction;

  int choice;
  uint32_t pcidx;
  uint32_t lhist;
  uint32_t ghistbits;

  ghistbits = global_mask & ghistory;
  choice = choice_pht[ghistbits];
  switch(choice){
    case 0:
      pcidx = pc_mask & pc;
      lhist = local_mask & lbht_tournament[pcidx];
      prediction = lpht_tournament[lhist];
      if (prediction > 1)
        return TAKEN;
      else
        return NOTTAKEN;
      break;
    case 1:
      pcidx = pc_mask & pc;
      lhist = local_mask & lbht_tournament[pcidx];
      prediction = lpht_tournament[lhist];
      if (prediction > 1)
        return TAKEN;
      else
        return NOTTAKEN;
      break;
    case 2:
      prediction = gpht_tournament[ghistbits];
      if (prediction > 1)
        return TAKEN;
      else
        return NOTTAKEN;
      break;
    case 3:
      prediction = gpht_tournament[ghistbits];
      if (prediction > 1)
        return TAKEN;
      else
        return NOTTAKEN;
      break;
    default:
      printf("Warning: Undefined state of entry in Chooser!\n");
  }
}


void
train_tournament(uint32_t pc, uint8_t outcome) {

  uint32_t ghistbits;
  uint32_t choice;
  uint32_t pcidx;
  uint32_t lhist;
  uint32_t local_pred;
  uint32_t global_pred;

  ghistbits = global_mask & ghistory;
  choice = choice_pht[ghistbits];
  pcidx = pc_mask & pc;
  lhist = local_mask & lbht_tournament[pcidx];
  local_pred = lpht_tournament[lhist];

  if (local_pred>1)
    local_pred = TAKEN;
  else
    local_pred = NOTTAKEN;
  global_pred = gpht_tournament[ghistbits];
  if (global_pred > 1)
    global_pred = TAKEN;
  else
    global_pred = NOTTAKEN;
  if (global_pred == outcome && local_pred != outcome && choice_pht[ghistbits] != 3)
    choice_pht[ghistbits]++;
  else if (global_pred != outcome && local_pred == outcome && choice_pht[ghistbits] != 0)
    choice_pht[ghistbits]--;



  
  if (outcome == 1)
  {
    if (gpht_tournament[ghistbits] != ST)
      gpht_tournament[ghistbits]++;
    if (lpht_tournament[lhist] !=ST)
      lpht_tournament[lhist]++;
  }
  else
  {
    if (gpht_tournament[ghistbits]!=SN)
      gpht_tournament[ghistbits]--;
    if (lpht_tournament[lhist]!=SN)
      lpht_tournament[lhist]--;
  }

  lbht_tournament[pcidx] = ((lbht_tournament[pcidx]<<1) | outcome) & local_mask;



  //Update history register
  ghistory = ((ghistory << 1) | outcome) & global_mask;  
  
  return;
}

//custom: perceptron

void perceptron_shift(int16_t* satuate, uint8_t same){
  // if the prediction is correct
  if(same){
    // make sure it is all 1s if not all 1s, improve by 1
    if(*satuate != ((1 << (SATUATELEN - 1)) - 1)){
      (*satuate)++;
    }
    // if not the same
  }else{
    // 
    if(*satuate != -(1 << (SATUATELEN - 1 ) )){
      (*satuate)--;
    }
  }
}

void init_custom(){
  //threshold
  threshold = (int32_t)(1.93 * HEIGHT + 14);

  //init size of weights and global history easier way compares with the init provided for gshare
  // referred to the implementation on
  //https://github.com/ajgupta93/gshare-and-tournament-branch-predictor/blob/master/src/percp.c
  memset(W, 0, sizeof(int16_t) * PCSIZE * (HEIGHT + 1));
  memset(ghistory_percep, 0, sizeof(uint16_t) * HEIGHT);
}


uint8_t custom_predict(uint32_t pc){
  uint32_t index = MASK_PC(pc);
  int16_t out = W[index][0];

  for(int i = 1 ; i <= HEIGHT ; i++)
  {
    if (ghistory_percep[i-1])
      out += W[index][i];
    else
      out -= W[index][i];
  }
  if (out>=0){
    recent_prediction =  TAKEN;
  }
  else
    recent_prediction =  NOTTAKEN;
  
  if (out < threshold && out > -threshold)
    need_train = 1;
  else
    need_train = 0;

  return recent_prediction;
}

void train_custom(uint32_t pc, uint8_t outcome){

  uint32_t index = MASK_PC(pc);

  if((recent_prediction != outcome) || need_train)
  {
    perceptron_shift(&(W[index][0]), outcome);
    for(int i = 1 ; i <= HEIGHT ; i++){
      uint8_t predict = ghistory_percep[i-1];
      perceptron_shift(&(W[index][i]), (outcome == predict));
    }

  }

  for(int i = HEIGHT - 1; i > 0 ; i--){
    ghistory_percep[i] = ghistory_percep[i-1];
  }
  ghistory_percep[0] = outcome;

}


void
init_predictor()
{
  switch (bpType) {
    case STATIC:
    case GSHARE:
      init_gshare();
      break;
    case TOURNAMENT:
      init_tournament();
      break;
    case CUSTOM:
      init_custom();
      break;
    default:
      break;
  }
  
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{

  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      return gshare_predict(pc);
    case TOURNAMENT:
      return tournament_predict(pc);
    case CUSTOM:
      return custom_predict(pc);
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void
train_predictor(uint32_t pc, uint8_t outcome)
{

  switch (bpType) {
    case STATIC:
    case GSHARE:
      return train_gshare(pc, outcome);
    case TOURNAMENT:
      return train_tournament(pc,outcome);
    case CUSTOM:
      return train_custom(pc,outcome);
    default:
      break;
  }
  

}
