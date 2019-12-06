#include "helloworld_t.h"
#include "ledger.h"

#include "signing-PB/signing.h"
#include "data_types.h"
#include "errcodes.h"

// eEVM
#include "eEVM/bigint.h"
#include "eEVM/opcode.h"
#include "eEVM/processor.h"
#include "eEVM/simple/simpleglobalstate.h"

	std::vector<uint8_t> create_bytecode(const std::string& s){
		std::vector<uint8_t> code;
		constexpr uint8_t mdest = 0x0;
		const uint8_t rsize = s.size() + 1;

		// Store each byte in evm memory
		uint8_t mcurrent = mdest;
		for (const char &c : s)
		{
			code.push_back(eevm::Opcode::PUSH1);
			code.push_back(c);
			code.push_back(eevm::Opcode::PUSH1);
			code.push_back(mcurrent++); // IH: this represents the address in the memory, starting from 0;
			code.push_back(eevm::Opcode::MSTORE8);
		}

		// Return
		code.push_back(eevm::Opcode::PUSH1);
		code.push_back(rsize); // the size to read from memory (i.e., length of string)
		code.push_back(eevm::Opcode::PUSH1);
		code.push_back(mdest); // starting from memory 0x00
		code.push_back(eevm::Opcode::RETURN);

		return code;
	}

	// int execute_bunch_of_txs(std::vector<eevm::Transaction> txs){

	// 	// eevm::Trace tr;
  	// 	// eevm::Processor p(gs);

	// 	// for (auto tx: txs){

	// 	// 	// Run the transaction
	// 	// 	// const auto exec_result = p.run(tx, from, gs.get(to), input, 0u, &tr);

	// 	// 	// if (exec_result.er != eevm::ExitReason::returned)
	// 	// 	// {
	// 	// 	// 	// Print the trace if nothing was returned
	// 	// 	// 	std::cerr << fmt::format("Trace:\n{}", tr) << std::endl;
	// 	// 	// 	if (exec_result.er == eevm::ExitReason::threw){
	// 	// 	// 		// Rethrow to highlight any exceptions raised in execution
	// 	// 	// 		throw std::runtime_error(fmt::format("Execution threw an error: {}", exec_result.exmsg));
	// 	// 	// 	}
	// 	// 	// 	throw std::runtime_error("Deployment did not return");
	// 	// 	// }
	// 	// }
	// }


	int ECLedger::execute_hello_world() {
		// Create random addresses for sender and contract
		std::vector<uint8_t> raw_address(20); // addrress has 20 Bytes
		std::generate(raw_address.begin(), raw_address.end(), []() { return std::rand(); });

		const eevm::Address sender =  eevm::from_big_endian(raw_address.data(), raw_address.size());

		std::generate(raw_address.begin(), raw_address.end(), []() { return std::rand(); });
		const eevm::Address to = eevm::from_big_endian(raw_address.data(), raw_address.size());


		// Create global state
		eevm::SimpleGlobalState gs;

		// Create code
		std::string hello_world("Hello world!");
		const eevm::Code code = create_bytecode(hello_world);

		// Deploy contract to global state
		uint256_t balance = 0;
		const eevm::AccountState contract = gs.create(to, balance, code);

		// Create transaction
		eevm::NullLogHandler ignore;
		eevm::Transaction tx(sender, ignore);

		// Create processor
		eevm::Processor p(gs);

		// Execute code. All executions are associated with a transaction. This
		// transaction is called by sender, executing the code in contract, with empty
		// input (and no trace collection)
		const eevm::ExecResult e = p.run(tx, sender, contract, {}, 0, nullptr);

		// Check the response
		if (e.er != eevm::ExitReason::returned)
		{
			// std::cout << fmt::format("Unexpected return code: {}", (size_t)e.er) << std::endl;
			return 2;
		}

		// Create string from response data, and print it
		const std::string response(reinterpret_cast<const char*>(e.output.data()));
		if (response != hello_world)
		{
			// throw std::runtime_error(fmt::format("Incorrect result.\n Expected: {}\n Actual: {}", hello_world, response));
			return 3;
		}
		return 0;
	}
