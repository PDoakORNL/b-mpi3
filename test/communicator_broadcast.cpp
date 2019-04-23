#if COMPILATION_INSTRUCTIONS
mpic++ -O3 -std=c++14 -Wall -Wfatal-errors $0 -o $0x.x && time mpirun -n 2 $0x.x $@ && rm -f $0x.x; exit
#endif
//  (C) Copyright Alfredo A. Correa 2018.
#include "../../mpi3/main.hpp"
#include "../../mpi3/communicator.hpp"

#include<iostream>

namespace mpi3 = boost::mpi3;
using std::cout;

int mpi3::main(int, char*[], mpi3::communicator world){

	std::vector<std::size_t> sizes = {100, 64*1024, 128*1024};
	int NUM_REPS = 5;

	using value_type = int;
	std::vector<value_type> buf(128*1024);

	for(std::size_t n=0; n != sizes.size(); ++n){
		if(world.root()) cout << "bcasting " << sizes[n] << " ints " << NUM_REPS << " times.\n";
		for(int reps = 0; reps != NUM_REPS; ++reps){
			if(world.root()) 
				for(std::size_t i=0; i != sizes[n]; ++i) 
					buf[i] = 1000000.*(n * NUM_REPS + reps) + i;
			else
				for(std::size_t i=0; i != sizes[n]; ++i) 
					buf[i] = -1 - (n * NUM_REPS + reps);

			world.broadcast_n(buf.begin(), sizes[n]);
		//	world.broadcast(buf.begin(), buf.begin() + sizes[n], 0);

			for(std::size_t i=0; i != sizes[n]; ++i) 
				assert( buf[i] == 1000000.*(n * NUM_REPS + reps) + i );
		}
	}

	return 0;
}

