#if COMPILATION_INSTRUCTIONS
mpic++ -Wall -Wextra -Wfatal-errors $0 -o $0x&&mpirun -n 4 $0x$@&&rm $0x;exit
#endif

#include "../../mpi3/main.hpp"
#include "../../mpi3/process.hpp"

namespace mpi3 = boost::mpi3;
using std::cout;

int mpi3::main(int, char*[], mpi3::communicator world){
	assert( world.size() ==  4 );

	std::vector<int> send_buff ={
		world.rank()*10 + 0,
		world.rank()*10 + 1,
		world.rank()*10 + 2,
		world.rank()*10 + 3
	};
	assert( (int)send_buff.size() == world.size() );

	std::vector<int> recv_buff(world.size(), 99.);
	{
		
		assert( (int)recv_buff.size() == world.size() );
		world.all_to_all( send_buff.begin(), recv_buff.begin() );
		
		if( world.rank() == 0 )
			assert( recv_buff[0] == 00 and recv_buff[ 1] == 10 and recv_buff[ 2] == 20 and recv_buff[ 3] == 30 );

		if( world.rank() == 1 )
			assert( recv_buff[0] == 01 and recv_buff[ 1] == 11 and recv_buff[ 2] == 21 and recv_buff[ 3] == 31 );
	}
	std::vector<int> recv_buff2(world.size(), 99.);
	for(std::size_t i = 0; i != send_buff.size(); ++i)
		world[i] << send_buff[i] >> recv_buff2[i];

	assert( recv_buff == recv_buff2 );

//	for(std::size_t i = 0; i != send_buff.size(); ++i)
//		world[i] << send_buff[i] >> send_buff[i];
	world & send_buff;
	assert( recv_buff == send_buff );

	return 0;
}

