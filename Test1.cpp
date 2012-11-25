// Coder : Sharad Bagri, sbagri@vt.edu

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

#define	HIGHEST_LVL	100
#define MAX_EVENTS_AT_LEVEL	100
#define MAX_LINE_CHAR	512
#define CKT_NAME_CHARS	40
#define MAX_FANIN_PER_GATE	20
#define	BKTRK_LMT	10000
#define MAX_LINE_RD_PATH_FILE	1000

enum
{
   G_JUNK,         /* 0 */
   G_INPUT,        /* 1 */
   G_OUTPUT,       /* 2 */
   G_XOR,          /* 3 */
   G_XNOR,         /* 4 */
   G_DFF,          /* 5 */
   G_AND,          /* 6 */
   G_NAND,         /* 7 */
   G_OR,           /* 8 */
   G_NOR,          /* 9 */
   G_NOT,          /* 10 */
   G_BUF,          /* 11 */
};

////////////////////////////////////////////////////////////////////////
// circuit class
////////////////////////////////////////////////////////////////////////

class circuit
{
    // circuit information
public:
	unsigned int numgates;		// total number of gates (faulty included)
	unsigned int maxlevels;		// number of levels in gate level ckt
	unsigned int numInputs;		// number of inputs
	unsigned int numOutputs;		// number of outputs
	int CountInvOnPathToPI;
	int ReturnValReqd;
	unsigned int DtctdPaths;
	unsigned int UnDtctblePths;
	unsigned int AbrtdPths;
	unsigned char *gtype;	// gate type
	short *fanin;		// number of fanins of gate
	short *fanout;		// number of fanouts of gate
	int *levelNum;		// level number of gate
	int **faninlist;	// fanin list of gate
	int **fanoutlist;	// fanout list of gate
	int **c_0_1;
//	int *Reqdgval;			// 0:0, 1:1, 2:X
	circuit(char *);	// constructor
	bool simulate(class Vect *, class Simul *);
	int btrace(int , int, Vect *);
};

class Vect
{
public:
	int Num;
	int Num_bktrk;
	int *PrevVal;
	int *NewVal;
	Vect(const int);
	bool UpdatePrevNew();
	bool PrintPrev() const;
	bool PrintNew() const;
	bool reset(int);
};

class path
{
public:
	unsigned int NoGates_ONPath;
	int *GNum_ONPath;

	unsigned int NoGates_OFFPath;
	int *GNum_OFFPath;
	int *OffPVal;
	unsigned int LineNoPathsFile;


	path();
	bool GetApath(FILE *,circuit *);		/*calls to this function will try its best to return only a true path,
								except gates invloving XOR, XNOR, that has to be handled
								by PODEM or something*/
	void PrintPath() const;
	void CleanPath();
	int CheckPathExcited(Vect *);	//-1 if yes, otherwise return gatenum to excite
	int PrintPIVect(circuit *, FILE *, Vect *);
	~path();
};

class Simul
{
public:
	int EvtQueue[HIGHEST_LVL][MAX_EVENTS_AT_LEVEL];
	int index[HIGHEST_LVL];
	int thislevel;
	int thisGateNum;
	bool ValueChanged;
	int highestEvtLevel;
	bool Flag_FoundInEvtQueue;
	bool ResetEvtQueue()
	{	for(int i=0;i<HIGHEST_LVL;i++)
		{	for(int j=0;j<MAX_EVENTS_AT_LEVEL;j++)
			{	EvtQueue[i][j] = 0;
			}
			index[i] = 0;
		}
		thislevel = -1;
		thisGateNum = -1;
		ValueChanged = 0;
		highestEvtLevel = -1;
		Flag_FoundInEvtQueue = 0;
		return 1;
	}

bool UpdateEventList(circuit *ckt1, Vect *SimVector)
{
	if(SimVector->NewVal[thisGateNum] != SimVector->PrevVal[thisGateNum])
	{	for(int k=0;k<ckt1->fanout[thisGateNum];k++)
		{
			int l;
			Flag_FoundInEvtQueue=0;
			int LevelOfOut = (ckt1->levelNum[ckt1->fanoutlist[thisGateNum][k]] / 5);
			for(l=0;l<index[LevelOfOut];l++)
			{	if(ckt1->fanoutlist[thisGateNum][k] == EvtQueue[LevelOfOut][l])
				{	Flag_FoundInEvtQueue=1;
					break;
				}
			}
			if(0 == Flag_FoundInEvtQueue)
			{	EvtQueue[LevelOfOut][l]=ckt1->fanoutlist[thisGateNum][k];
				index[LevelOfOut]++;
				if(highestEvtLevel<LevelOfOut)
				{	highestEvtLevel = LevelOfOut;
				}
			}
		}
	}
	return 1;
}

};


int recurexcite(class path *, class Vect *, class circuit *, class Simul *);


////////////////////////////////////////////////////////////////////////
// constructor: reads in the *.lev file and builds basic data structure
// 		for the gate-level ckt
////////////////////////////////////////////////////////////////////////
circuit::circuit(char *cktName)
{
    FILE *inFile;
    char fName[40];
    int i, j, count;
    char c;
    int gatenum, junk;
    int f1, f2, f3, f4, f5;

    strcpy(fName, cktName);
    strcat(fName, ".lev");
    inFile = fopen(fName, "r");
    if (inFile == NULL)
    {
	fprintf(stderr, "Can't open .lev file\n");
	exit(-1);
    }

    numgates = maxlevels = 0;
    numInputs = numOutputs = 0;

    fscanf(inFile, "%d", &count);	// number of gates
    fscanf(inFile, "%d", &junk);	// skip the second line

    // allocate space for gates data structure
    gtype = new unsigned char[count];
    fanin = new short[count];
    fanout = new short[count];
    levelNum = new int[count];
    faninlist = new int * [count];
    fanoutlist = new int * [count];
	c_0_1 = new int * [count];
//	gval = new int[count];

    // now read in the circuit
    for (i=1; i<count; i++)
    {
	fscanf(inFile, "%d", &gatenum);
	fscanf(inFile, "%d", &f1);
	fscanf(inFile, "%d", &f2);
	fscanf(inFile, "%d", &f3);

	numgates++;
	gtype[gatenum] = (unsigned char) f1;
	if (gtype[gatenum] > 13)
	    printf("gate %d is an unimplemented gate type\n", gatenum);
	else if (gtype[gatenum] == G_INPUT)
	    numInputs++;
	else if (gtype[gatenum] == G_OUTPUT)
	    numOutputs++;

	f2 = (int) f2;
	levelNum[gatenum] = f2;

	if (f2 >= (maxlevels))
	    maxlevels = f2 + 5;

	fanin[gatenum] = (int) f3;
	// now read in the faninlist
	faninlist[gatenum] = new int[fanin[gatenum]];
	for (j=0; j<fanin[gatenum]; j++)
	{
	    fscanf(inFile, "%d", &f1);
	    faninlist[gatenum][j] = (int) f1;
	}

	for (j=0; j<fanin[gatenum]; j++) // followed by samethings
	    fscanf(inFile, "%d", &junk);

	// read in the fanout list
	fscanf(inFile, "%d", &f1);
	fanout[gatenum] = (int) f1;

	fanoutlist[gatenum] = new int[fanout[gatenum]];
	for (j=0; j<fanout[gatenum]; j++)
	{
	    fscanf(inFile, "%d", &f1);
	    fanoutlist[gatenum][j] = (int) f1;
	}

	while(c = getc(inFile))
		{if ((c == ';') || (c=='O'))	break;
		}

	c_0_1[gatenum] = new int[2];
	fscanf(inFile, "%d", &f4);
	c_0_1[gatenum][0] = (int) f4;
	fscanf(inFile, "%d", &f5);
	c_0_1[gatenum][1] = (int) f5;

	// skip till end of line
	while ((c = getc(inFile)) != '\n' && c != EOF)
	    ;
    }	// for (i...)
    fclose(inFile);

	DtctdPaths = 0;
	UnDtctblePths = 0;
	AbrtdPths = 0;

  printf("Successfully read in circuit:\n");
    printf("\t%d PIs.\n", numInputs);
    printf("\t%d POs.\n", numOutputs);
    printf("\t%d total number of gates\n", numgates);
    printf("\t%d levels in the circuit.\n", maxlevels / 5);

if((maxlevels / 5) > HIGHEST_LVL)
    {
	fprintf(stderr, "Array path's size is smaller than maximum level, increase HIGHEST_LVL value in code\n");
	exit(21);
    }
}


bool circuit::simulate(Vect *SimVector, Simul *Sim)
{
unsigned int i,j;
for(i=0;i<HIGHEST_LVL;i++)
{	Sim->index[i]=0;
}

//in the beginning schedule only those gates whose Previous Value is not same as new value
for(i=1;i<=numgates;i++)
{	if (SimVector->NewVal[i] != SimVector->PrevVal[i])
	{	Sim->thislevel = levelNum[i]/5;
		//add to event queue
		Sim->EvtQueue[Sim->thislevel][Sim->index[Sim->thislevel]]=i;
		//saving the gate number at the level where it is to be simulated.
		//index[thislevel] is number of events stored at this level;
		Sim->index[Sim->thislevel]++;
		//for checking possible fault, can be removed in final prg to increase speed
		if(MAX_EVENTS_AT_LEVEL<(Sim->index[Sim->thislevel]))
		{	printf("Increase MAX_EVENTS_AT_LEVEL\n");
			exit(-1);
		}
		if(Sim->highestEvtLevel<Sim->thislevel)
		{	Sim->highestEvtLevel = Sim->thislevel;
		}
		Sim->ValueChanged = 1;
	}
}

Sim->thisGateNum = -1;

while(0 != Sim->ValueChanged)
{	Sim->ValueChanged = 0;
	for(i=0;i<=Sim->highestEvtLevel;i++)
	{	for(j=0;j<Sim->index[i];j++)
		{	Sim->thisGateNum = Sim->EvtQueue[i][j];
			if((Sim->thisGateNum < 0) || (Sim->thisGateNum > numgates))
			{	printf("Not a valid gate on Event queue[%d][%d] gate num is %d",i,j,Sim->thisGateNum);
				exit(22);	//not a valid gate on Event Queue
			}
			{	switch(gtype[Sim->thisGateNum])
				{
				case G_JUNK/*0*/	:
					printf("Gate Type: G_JUNK\n"); break;
				case G_INPUT/*1*/	:
					{	//printf("Gate Type G_INPUT, Gate No %d\n",Sim->thisGateNum);

						Sim->UpdateEventList(this, SimVector);
					}
					break;
				case G_OUTPUT/*2*/	:
					{
						SimVector->NewVal[Sim->thisGateNum] = \
							SimVector->NewVal[faninlist[Sim->thisGateNum][0]];/*value of whatever is feeding to the input*/;
						//printf("Yahoo Reached an output, just updated without scheduling an event\n");
					//	SimVector->PrintNew();
					}
					break;
				case G_XOR/*3*/		:
					{	//printf("Gate Type G_XOR, Gate No %d\n",Sim->thisGateNum);
						int InterResult = SimVector->NewVal[faninlist[Sim->thisGateNum][0]];
						for (int k=1;k<fanin[Sim->thisGateNum];k++)
						{	switch(SimVector->NewVal[faninlist[Sim->thisGateNum][k]])
							{	case -1:switch(InterResult)
										{
											case -1:
											case 0:
											case 1: InterResult = -1; break;
											default: printf("InterResult not a valid value\n");
										}
										break;
								case 0:switch(InterResult)
										{
											case -1: InterResult = -1; break;
											case 0: InterResult = 0; break;
											case 1: InterResult = 1; break;
											default: printf("InterResult not a valid value\n");
										}
									   break;
								case 1:switch(InterResult)
										{
											case -1: InterResult = -1; break;
											case 0: InterResult = 1; break;
											case 1: InterResult = 0; break;
											default: printf("InterResult not a valid value\n");
										}
									   break;
								default: printf("Not a valid SimVector->NewVal[faninlist[Sim->thisGateNum][k]]\n");
									break;
							}
							if(-1 == InterResult)	break;
						}
						SimVector->NewVal[Sim->thisGateNum] = InterResult;
						Sim->UpdateEventList(this, SimVector);
					}
					break;
				case G_XNOR/*4*/	:
					{	printf("Gate Type G_XNOR, Gate No %d\n",Sim->thisGateNum);
						int InterResult = SimVector->NewVal[faninlist[Sim->thisGateNum][0]];
						for (int k=1;k<fanin[Sim->thisGateNum];k++)
						{	switch(SimVector->NewVal[faninlist[Sim->thisGateNum][k]])
							{	case -1:switch(InterResult)
										{
											case -1:
											case 0:
											case 1: InterResult = -1; break;
											default: printf("InterResult not a valid value\n");
										}
										break;
								case 0:switch(InterResult)
										{
											case -1: InterResult = -1; break;
											case 0: InterResult = 1; break;
											case 1: InterResult = 0; break;
											default: printf("InterResult not a valid value\n");
										}
									   break;
								case 1:switch(InterResult)
										{
											case -1: InterResult = -1; break;
											case 0: InterResult = 0; break;
											case 1: InterResult = 1; break;
											default: printf("InterResult not a valid value\n");
										}
									   break;
								default: printf("Not a valid SimVector->NewVal[faninlist[Sim->thisGateNum][k]]\n");
									break;
							}
							if(-1 == InterResult)	break;
						}
						SimVector->NewVal[Sim->thisGateNum] = InterResult;
						Sim->UpdateEventList(this, SimVector);
					}
					break;
				case G_DFF/*5*/		:
					printf("Gate Type G_DFF (Not Implemented), Gate No %d\n",Sim->thisGateNum);
					exit(27);
					break;
				case G_AND/*6*/		:
					{	//printf("Gate Type G_AND, Gate No %d\n",Sim->thisGateNum);
						int InterResult=1;
						for (int k=0;k<fanin[Sim->thisGateNum];k++)
						{	if(SimVector->NewVal[faninlist[Sim->thisGateNum][k]] == 0)
							{	InterResult = 0;
								break;
							}
							else if(SimVector->NewVal[faninlist[Sim->thisGateNum][k]] == 1)
							{	if(0 == InterResult)
									InterResult = 0;
								else if(1 == InterResult)
								{	InterResult = 1;
								}
								else InterResult = -1;
							}
							else
							{	InterResult = -1;
							}
						}
						SimVector->NewVal[Sim->thisGateNum] = InterResult;
						Sim->UpdateEventList(this, SimVector);
					}
					break;
				case G_NAND/*7*/	:
					{	//printf("Gate Type G_NAND, Gate No %d\n",Sim->thisGateNum);
						int InterResult=0;
						for (int k=0;k<fanin[Sim->thisGateNum];k++)
						{	if(SimVector->NewVal[faninlist[Sim->thisGateNum][k]] == 0)
							{	InterResult = 1;
								break;
							}
							else if(SimVector->NewVal[faninlist[Sim->thisGateNum][k]] == 1)
							{	if(0 == InterResult)
									InterResult = 0;
								else InterResult = -1;
							}
							else
							{	InterResult = -1;
							}
						}
						SimVector->NewVal[Sim->thisGateNum] = InterResult;
						Sim->UpdateEventList(this, SimVector);
					}
					break;
				case G_OR/*8*/		:
					{	//printf("Gate Type G_OR, Gate No %d\n",Sim->thisGateNum);
						int InterResult=0;
						for (int k=0;k<fanin[Sim->thisGateNum];k++)
						{	if(SimVector->NewVal[faninlist[Sim->thisGateNum][k]] == 1)
							{	InterResult = 1;
								break;
							}
							else if(SimVector->NewVal[faninlist[Sim->thisGateNum][k]] == 0)
							{	if(0 == InterResult)
									InterResult = 0;
								else InterResult = -1;
							}
							else
							{	InterResult = -1;
							}
						}
						SimVector->NewVal[Sim->thisGateNum] = InterResult;
						Sim->UpdateEventList(this,SimVector);
					}
					break;
				case G_NOR/*9*/		:
					{	//printf("Gate Type G_NOR, Gate No %d\n",Sim->thisGateNum);
						int InterResult=0;
						for (int k=0;k<fanin[Sim->thisGateNum];k++)
						{	if(SimVector->NewVal[faninlist[Sim->thisGateNum][k]] == 1)
							{	InterResult = 0;
								break;
							}
							else if(SimVector->NewVal[faninlist[Sim->thisGateNum][k]] == 0)
							{	if(0 == InterResult)
								{	InterResult = 1;
								}
								else if(1 == InterResult)
								{	InterResult = 1;
								}
								else
								{	InterResult = -1;
								}
							}
							else
							{	InterResult = -1;
							}
						}
						SimVector->NewVal[Sim->thisGateNum] = InterResult;
						Sim->UpdateEventList(this, SimVector);
					}
					break;
				case G_NOT/*10*/	:
					{	//printf("Gate Type G_NOT, Gate No %d\n",Sim->thisGateNum);
						if(fanin[Sim->thisGateNum]>1)
						{	printf("More than 1 input to NOT gate\n");
							exit(23);
						}
						switch(SimVector->NewVal[faninlist[Sim->thisGateNum][0]])
						{	case -1:	SimVector->NewVal[Sim->thisGateNum] = -1; break;
							case 0:	SimVector->NewVal[Sim->thisGateNum] = 1; break;
							case 1:	SimVector->NewVal[Sim->thisGateNum] = 0; break;
							default: printf("Unknown Input value to NOT gate\n");
						}
						Sim->UpdateEventList(this, SimVector);
					}
					break;
				case G_BUF/*11*/	:
					{//	printf("Gate Type G_BUF, Gate No %d\n",Sim->thisGateNum);
						if(fanin[Sim->thisGateNum]>1)
						{	printf("More than 1 input to BUFFER gate\n");
							exit(24);
						}
						switch(SimVector->NewVal[faninlist[Sim->thisGateNum][0]])
						{	case -1:	SimVector->NewVal[Sim->thisGateNum] = -1; break;
							case 0:	SimVector->NewVal[Sim->thisGateNum] = 0; break;
							case 1:	SimVector->NewVal[Sim->thisGateNum] = 1; break;
							default: printf("Unknown Input value to BUFFER gate\n");
						}
						Sim->UpdateEventList(this, SimVector);
					}
					break;
				default	: printf("Unknown gate type\n"); exit(-1); break;
				}
			}
		}
//		SimVector->PrintNew();
	}
}
SimVector->UpdatePrevNew();
}


int circuit::btrace(int GateNum, int ValReqd, Vect *EventVect)
{
	int InterGNum = GateNum;
	int i,j;
	int temp, NumUnassignedIps;
	int FanIns[MAX_FANIN_PER_GATE];	//reamining fanins which haven't been assigned a value
	int RetGateNum = -1;


	CountInvOnPathToPI = 0;
	if(gtype[GateNum] != G_INPUT)
	{
		if(MAX_FANIN_PER_GATE < fanin[GateNum])
		{	printf("Increase MAX_FANIN_PER_GATE to atleast %d\n",fanin[GateNum]);
			exit(28);
		}
		NumUnassignedIps = 0;

		for(i=0;i<fanin[GateNum];i++)
		{	if(-1 == EventVect->NewVal[faninlist[GateNum][i]])
			{	FanIns[NumUnassignedIps] = faninlist[GateNum][i];
				NumUnassignedIps++;
			}
		}
		if(0 == NumUnassignedIps)
		{	RetGateNum =-1;
			return RetGateNum;
		}

	//to select lower of c0 or c1
		for(i=0;i<NumUnassignedIps;i++)
		{	for(j=i+1;j<NumUnassignedIps;j++)
			{	switch(ValReqd)
				{	case 0:
						if((c_0_1[FanIns[i]][0]) > (c_0_1[FanIns[j]][0]))
						{	temp = FanIns[i];
							FanIns[i] = FanIns[j];
							FanIns[j] = temp;
						}
						break;
					case 1:
						if((c_0_1[FanIns[i]][1]) > (c_0_1[FanIns[j]][1]))
						{	temp = FanIns[i];
							FanIns[i] = FanIns[j];
							FanIns[j] = temp;
						}
						break;
					default: printf("Wrong ValReqd = %d\n",ValReqd);
						exit(29);
				}
			}
		}

		for(i=0;i<NumUnassignedIps;i++)
		{
			if(/*(gtype[GateNum] == G_XNOR) ||*/ (gtype[GateNum] == G_NAND)\
					|| (gtype[GateNum] == G_NOR) || (gtype[GateNum] == G_NOT))
			{	switch(ValReqd)
				{	case 0: ValReqd = 1;	break;
					case 1:	ValReqd = 0;	break;
					//not checking for default case as already checked above
				}
			}
			RetGateNum = btrace(FanIns[i], ValReqd, EventVect);
			if(-1 == RetGateNum)
			{	continue;
			}
			else
			{	if(/*(gtype[FanIns[i]] == G_XNOR) ||*/ (gtype[FanIns[i]] == G_NAND)\
					|| (gtype[FanIns[i]] == G_NOR) || (gtype[FanIns[i]] == G_NOT))
					CountInvOnPathToPI++;

				return RetGateNum;
			}
			//can't be all as -1 as it is checked in beginning, so not putting code to check that condition
		}

	}
	else
	{	RetGateNum = GateNum;
		ReturnValReqd = ValReqd;
		if((gtype[GateNum] == G_XNOR) || (gtype[GateNum] == G_NAND)\
			|| (gtype[GateNum] == G_NOR) || (gtype[GateNum] == G_NOT))
			CountInvOnPathToPI++;
		return RetGateNum;	//No Unassigned Gates remain
	}
	return -1;
}

Vect::Vect(const int Number)
{	int i;
	Num = Number;
	PrevVal = new int[Number+1];
	NewVal = new int[Number+1];
	for(i=1;i<=Number;i++)
	{	PrevVal[i]=-1;
		NewVal[i]=-1;
	}
	Num_bktrk = 0;
}

bool Vect::UpdatePrevNew()
{
	int i;
	for(i=1;i<=Num;i++)
		PrevVal[i] = NewVal[i];
	return 1;
}

bool Vect::PrintPrev() const
{
	int i;
	for(i=1;i<=Num;i++)
	{if(-1 == PrevVal[i])
		{
		printf("X ");
		}
	else
		{
		printf("%d ",PrevVal[i]);
		}
	}
	printf("\n");
	return 1;
}

bool Vect::PrintNew() const
{
	int i;
	for(i=1;i<=Num;i++)
		{if(-1 == NewVal[i])
			{
			printf("X ");
			}
		else
			{
			printf("%d ",NewVal[i]);
			}
		}
	printf("\n");
	return 1;
}

bool Vect::reset(int ResetVal)
{
	for(int i=1;i<=Num;i++)
	{	NewVal[i] = ResetVal;
		PrevVal[i] = ResetVal;
	}
	Num_bktrk = 0;
return 1;
}


path::path()
{	NoGates_ONPath = 0;
	GNum_ONPath = NULL;

	NoGates_OFFPath = 0;
	GNum_OFFPath = NULL;
	OffPVal = NULL;
	LineNoPathsFile = 0;
}

bool path::GetApath(FILE *inFile2, circuit *ckt)
{
	char line[MAX_LINE_CHAR];
	unsigned int GateList[MAX_LINE_CHAR];
	unsigned int NoOfGatesOnPath;			//no of gates in a line
	unsigned int j,k,l,m,n,o;
	unsigned int type=2;		//index to sensitization value for a gate
	unsigned int FanInGateNo;
	unsigned int SnIpLst[MAX_LINE_CHAR];	//Sensitization Gates No.s
	unsigned int SnValLst[MAX_LINE_CHAR];	//Value for Sensitization of Gate
	unsigned int NoOfGateOnSIP=0;	//No. of Sensitized gates
	bool FalsePath;

	if((fgets(line, sizeof(line), inFile2) != NULL) && (LineNoPathsFile<MAX_LINE_RD_PATH_FILE))
	{	LineNoPathsFile++;
		j=0;	//Counter of input line
		l=0; 	//Counts No. of Gates on sensitized path
		m=0;	//Counts No. of Inputs for a gate

		NoOfGatesOnPath = atoi(line);	//1st element is count of how many gates to expect
		while(' '!=line[j]) j++; j++;
		for(k=0;k<NoOfGatesOnPath;k++)
		{
			GateList[k]=atoi(&line[j]);
			while(' '!=line[j]) j++; j++;

			type=2;
			switch(ckt->gtype[GateList[k]])
			{
				case 1:	break;			//Primary Input
				case 2:	break;			//Primary Output
				case 3:	break;	//XOR, No fixed Sensitization Value
				case 4:	break;	//XNOR
				case 5:	break;			//dff
				case 6:	type=1;	break;	//AND
				case 7:	type=1;	break;	//NAND
				case 8:	type=0; break;	//OR
				case 9:	type=0;	break;	//NOR
				case 10: break;			//NOT gate
				case 11: break;			//buffer
				default:printf("Unknown Gate type, Gate No %d\n",GateList[k]);
						break;
			}

			for(m=0;m < (unsigned int)ckt->fanin[GateList[k]]; m++)
			{
				FanInGateNo = (unsigned int) ckt->faninlist[GateList[k]][m];
				if(FanInGateNo != GateList[k-1])
				{
					switch(type)
					{
						case 0:	SnIpLst[l]=FanInGateNo;	//OR or NOR gate
								SnValLst[l]=0;
								l++;
								break;
						case 1:	SnIpLst[l]=FanInGateNo;	//AND or NAND gate
								SnValLst[l]=1;
								l++;
								break;
						case 2: break;	//Gate with undefined value
					}
				}
			}
		}

		NoOfGateOnSIP = l;
		FalsePath = 0;

		for(l=0;l<NoOfGateOnSIP;l++)
		{
			for(o=l+1;o<NoOfGateOnSIP;o++)
			{
				if(SnIpLst[l]==SnIpLst[o])
				{
					if(SnValLst[l]!=SnValLst[o])
					{	FalsePath = 1;
						break;
					}
				}
			}
		}
	}
	else
	{	return 0;	//EOF reached or no. of lines read
	}

	if(1 != FalsePath)
	{	NoGates_ONPath = NoOfGatesOnPath;
		GNum_ONPath = new int[NoGates_ONPath];
		if(NULL == GNum_ONPath)
		{	printf("Dynamic Memory Allocation failed\n");
			exit(25);
		}
		for(n=0;n<NoGates_ONPath;n++)
		{	GNum_ONPath[n] = GateList[n];
		}

		NoGates_OFFPath = NoOfGateOnSIP;
		GNum_OFFPath = new int[NoGates_OFFPath];
		OffPVal = new int[NoGates_OFFPath];
		if(NULL == GNum_OFFPath || NULL == OffPVal)
		{	printf("Dynamic Memory Allocation failed\n");
			exit(26);
		}
		for(n=0;n<NoGates_OFFPath;n++)
		{	GNum_OFFPath[n] = SnIpLst[n];
			OffPVal[n] = SnValLst[n];
		}
	}
	else
	{	if(0 == GetApath(inFile2, ckt));//If again get a false path, call GetApath again
			return 0;	//if reached end of file after false path, return 0
	}
	return 1;
}

void path::PrintPath() const
{
for(int n=0;n<NoGates_ONPath;n++)
	printf("%d ",GNum_ONPath[n]);
printf("\n");

for(int n=0;n<NoGates_OFFPath;n++)
	printf("%d=%d ",GNum_OFFPath[n],OffPVal[n]);
printf("\n");
}

void path::CleanPath()
{
free(GNum_ONPath);
free(GNum_OFFPath);
free(OffPVal);
}

int path::CheckPathExcited(Vect *EventVect)
{
	int n;
	for(n=0;n<NoGates_OFFPath;n++)
	{	if(EventVect->NewVal[GNum_OFFPath[n]] != OffPVal[n])
		{	if(EventVect->NewVal[GNum_OFFPath[n]] != -1)
				return 0;	//conflict of existing value on gate and reqd. value
			else
				return GNum_OFFPath[n];
		}

	}
	return -1;
};

int path::PrintPIVect(circuit *ckt, FILE *OutFile, Vect *EventVect)
{
	for(int i=1;i<=ckt->numInputs;i++)
	{	if(-1 != EventVect->PrevVal[i])
		{	fprintf(OutFile, "%d ", EventVect->PrevVal[i]);
		}
		else
		{	fprintf(OutFile, "X ");
		}

	}

	fprintf(OutFile, "\n\t");

	for(int i=1;i<=ckt->numInputs;i++)
	{	if(-1 != EventVect->NewVal[i])
		{	fprintf(OutFile, "%d ", EventVect->NewVal[i]);
		}
		else
		{	fprintf(OutFile, "X ");
		}

	}
	fprintf(OutFile, "\n");
	return 1;
}

path::~path()
{
}


//////////////////////////////////////////////////////////////////////
// main starts here
//////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{

char cktName[CKT_NAME_CHARS];
circuit *ckt;
Vect *EventVect;
class Simul *Sim;
class path *ofpath;
FILE *inFile2;
char fName2[CKT_NAME_CHARS];
FILE *OutFile;
char OutCktName[CKT_NAME_CHARS];


if (argc != 2)
{
	fprintf(stderr, "Usage: %s <ckt>\n", argv[0]);
	exit(-1);
}

strcpy(cktName, argv[1]);

// read in the circuit and build data structure
ckt = new circuit(cktName);

//Reading *.paths file
strcpy(fName2, cktName);
strcat(fName2, ".paths");
inFile2 = fopen(fName2, "r");
if (inFile2 == NULL)
{
	fprintf(stderr, "Can't open .paths file\n");
	exit(20);
}

strcpy(OutCktName, cktName);
strcat(OutCktName, ".vec");
OutFile = fopen(OutCktName, "w");
if(NULL == OutFile)
{	printf("Can't create Output File");
	exit(30);
}

Sim = new Simul;
ofpath = new path;
EventVect = new Vect(ckt->numgates);

int PIGate;
while(0 != ofpath->GetApath(inFile2, ckt))
{
	EventVect->reset(-1);
	Sim->ResetEvtQueue();
	// Vec1 is NewVal
	//set from 0 to 1
	EventVect->PrevVal[ofpath->GNum_ONPath[0]] = 0;
	EventVect->NewVal[ofpath->GNum_ONPath[0]] = 1;

	int success = -2;
	success = recurexcite(ofpath, EventVect, ckt, Sim);
	if(1 == success)
	{	ofpath->PrintPIVect(ckt, OutFile, EventVect);
		ckt->DtctdPaths++;
		printf("rising path %d\n",(ofpath->LineNoPathsFile - 1));
	}
	else if(0 == success)
	{	printf("Not excited Rising path %d\n", (ofpath->LineNoPathsFile - 1));
		ckt->UnDtctblePths++;
	}
	else
	{	ckt->AbrtdPths++;
		printf("Aborted Rising path %d\n", (ofpath->LineNoPathsFile - 1));
	}


	//set from 1 to 0
	EventVect->reset(-1);
	Sim->ResetEvtQueue();
	EventVect->PrevVal[ofpath->GNum_ONPath[0]] = 1;
	EventVect->NewVal[ofpath->GNum_ONPath[0]] = 0;

	success = -2;
	success = recurexcite(ofpath, EventVect, ckt, Sim);
	if(1 == success)
	{	ofpath->PrintPIVect(ckt, OutFile, EventVect);
		ckt->DtctdPaths++;
		printf("falling path %d\n",(ofpath->LineNoPathsFile - 1));
	}
	else if(0 == success)
	{	printf("not excited Falling %d\n", (ofpath->LineNoPathsFile - 1));
		ckt->UnDtctblePths++;
	}
	else
	{	printf("Aborted Falling %d\n", (ofpath->LineNoPathsFile - 1));
		ckt->AbrtdPths++;
	}


	ofpath->CleanPath();
}

printf("Number of detected paths = %d\n", ckt->DtctdPaths);
printf("Number of undetectable paths = %d\n", ckt->UnDtctblePths);
printf("Number of aborted paths = %d\n", ckt->AbrtdPths);

fclose(OutFile);
fclose(inFile2);
free(EventVect);
free(ofpath);
free(ckt);
free(Sim);
return 1;
}


int recurexcite(class path *ofpath, class Vect *EventVect, class circuit *ckt, class Simul *Sim)
{	int GateToTrace = -1;
	int PIGate = -1;
	GateToTrace = ofpath->CheckPathExcited(EventVect);
	if(-1 == GateToTrace)
	{	return 1;	}
	else if(BKTRK_LMT < (EventVect->Num_bktrk))
	{	return (-1);	}
	else if(0 == GateToTrace)
	{	return 0;	}
	else
	{	int FlagGateFound = 0;
		for(unsigned int i=0; i < ofpath->NoGates_OFFPath; i++)
		{	if(ofpath->GNum_OFFPath[i] == GateToTrace)
			{	PIGate = ckt->btrace(GateToTrace, ofpath->OffPVal[i], EventVect);
				if(-1 == PIGate)
				{	return -1;
				}
				FlagGateFound = 1;
				break;
			}
		}
		if(0 == FlagGateFound)
		{	printf("Gate to be found doesn't go till a PI\n");
			return -1;
		}
		int v;
		v = ckt->ReturnValReqd;
		EventVect->NewVal[PIGate] = v;	//this is updated by btrace
		ckt->simulate(EventVect, Sim);

		if(1 == recurexcite(ofpath, EventVect, ckt, Sim))
		{	return 1;
		}
		else
		{
			EventVect->NewVal[PIGate] = (v==0?1:0);
			ckt->simulate(EventVect, Sim);
			if(1 == recurexcite(ofpath, EventVect, ckt, Sim))
			{	return 1;
			}
			else
			{	EventVect->NewVal[PIGate] = -1;
				ckt->simulate(EventVect, Sim);
				EventVect->Num_bktrk++;
				if(EventVect->Num_bktrk > BKTRK_LMT)
					return -1;
				else
					return 0;
			}
		}
	}
}
