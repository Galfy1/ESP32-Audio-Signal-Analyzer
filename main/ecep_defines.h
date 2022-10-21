

#ifndef ECEP_DEFINES_H_INCLUDED
#define ECEP_DEFINES_H_INCLUDED










typedef struct
{
    int blaVar1;
    float blaVar2;
} s_event1_t;

typedef struct
{
    int blaVarr1;
    float blaVarr2;
    float blaVarr3;
} s_event2_t;



typedef union
{
    s_event1_t wee;
    s_event2_t wee2;
} ecep_events;


// Structure for queues:
typedef struct
{
    int Event  // Hvilket event det er (eventkoden) Så tasken ved hvordan den skal læse uEvents 

    ecep_events Params;     

} s_BlaBlaStruct_t;








#endif /*ECEP_DEFINES_H_INCLUDED */
