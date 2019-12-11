#ifndef EEVM_LEDGER
#define EEVM_LEDGER


class ECLedger{

  public:
	ECLedger(){};
	int execute_hello_world(void);
	int execute_sum_a_b(int a, int b);
};

#endif // EEVM_LEDGER
