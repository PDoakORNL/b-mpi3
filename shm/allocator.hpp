#if COMPILATION_INSTRUCTIONS//-*-indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4;-*-
mpic++ -D_TEST_MPI3_SHM_ALLOCATOR -xc++ $0 -o $0x&&mpirun -n 3 $0x&&rm $0x;exit
#endif
// © Alfredo A. Correa 2019-2020

#ifndef MPI3_SHM_ALLOCATOR_HPP
#define MPI3_SHM_ALLOCATOR_HPP

//#include "../../mpi3/shared_window.hpp"
#include "../shm/pointer.hpp"

namespace boost{
namespace mpi3{
namespace shm{

template<class T = void>
struct allocator{
	template<class U> struct rebind{typedef allocator<U> other;};
	using value_type = T;
	using pointer = shm::pointer<T>;
	using const_pointer = shm::pointer<T const>;
	using size_type = mpi3::size_t;
	using difference_type = mpi3::ptrdiff_t;
private:
	mpi3::shared_communicator* comm_;
//	allocator() = delete;
public:
	allocator(mpi3::shared_communicator* comm = static_cast<mpi3::shared_communicator*>(&mpi3::environment::get_self_instance())) : comm_{comm}{}
	template<class U> allocator(allocator<U> const& o) : comm_{o.comm_}{}
	~allocator() = default;
	pointer allocate(size_type n, const void* /*hint*/ = 0) const{
		pointer ret = 0;
	// this code using reset is to workaround a bug in outdated libc++ https://bugs.llvm.org/show_bug.cgi?id=42021
		ret.w_.reset(new shared_window<T>{
			comm_->make_shared_window<T>(comm_->root()?n:0)
		});
		ret.w_->lock_all();
	//	ret.w_ = std::make_shared<shared_window<T>>(
	//		comm_->make_shared_window<T>(comm_->root()?n:0)
	//	);
		return ret;
	}
	void deallocate(pointer p, size_type){
		p.w_->unlock_all();
		p.w_.reset();
	}
	allocator& operator=(allocator const& other) = default;
	bool operator==(allocator const& other) const{return comm_==other.comm_;}
	bool operator!=(allocator const& other) const{return not(other == *this);}
	template<class... As>
	void construct(pointer p, As&&... as){
		if(comm_->root()) ::new((void*)raw_pointer_cast(p)) value_type(std::forward<As>(as)...);
	}
#if 0
	template<class P = pointer> void destroy(P p){
		using V = typename std::pointer_traits<P>::element_type;
		p->~V();
	}

#endif
	void destroy(pointer p){if(comm_->root()) raw_pointer_cast(p)->~value_type();}
	template<class TT, class Size, class TT1>
	auto alloc_uninitialized_fill_n(ptr<TT> it, Size n, TT1 const& t){
		if(comm_->root()) std::uninitialized_fill_n(raw_pointer_cast(it), n, t); // TODO implement with with for_each in parallel
		it.w_->fence();
		it.w_->fence();
		comm_->barrier();
	}
	template<class InputIt, class Size, class TT>
	auto alloc_uninitialized_copy_n(InputIt first, Size count, ptr<TT> d_first){
		return alloc_uninitialized_copy_n(first, count, raw_pointer_cast(d_first));
		if(comm_->root()) std::uninitialized_copy_n(first, count, raw_pointer_cast(d_first)); // TODO implement with with for_each in parallel
		d_first.w_->fence();
		comm_->barrier();		
	}
	template<class TT, class Size, class ForwardIt, std::enable_if_t<not is_a_ptr<ForwardIt>{}, int> = 0>
	auto alloc_uninitialized_copy_n(ptr<TT> first, Size count, ForwardIt d_first){
		first.w_->fence();
		if(comm_->root()) std::uninitialized_copy_n(raw_pointer_cast(first), count, d_first); // TODO implement with with for_each in parallel
		comm_->barrier();		
	}
	template<class TT1, class Size, class TT2>
	auto alloc_uninitialized_copy_n(ptr<TT1> first, Size count, ptr<TT2> d_first){
		first.w_->fence();
		if(comm_->root()) std::uninitialized_copy_n(raw_pointer_cast(first), count, raw_pointer_cast(d_first)); // TODO implement with with for_each in parallel
		d_first.w_->fence();
		comm_->barrier();		
	}
	template<class Ptr, class Size, class TT = typename std::pointer_traits<Ptr>::element_type>
	auto alloc_destroy_n(Ptr first, Size count){
		first.w_->fence();
		if(comm_->root())
			for( ; count > 0; --count, ++first) raw_pointer_cast(first)->~TT();
		first.w_->fence();
		comm_->barrier();
		return first + count;
	}
};

}}
}

#ifdef _TEST_MPI3_SHM_ALLOCATOR

#include "../../mpi3/main.hpp"
#include "../../mpi3/shm/allocator.hpp"

namespace mpi3 = boost::mpi3;

int mpi3::main(int, char*[], mpi3::communicator world){

	mpi3::shared_communicator node = world.split_shared();

	mpi3::shm::allocator<double> A1(&node);
	mpi3::shm::pointer<double> data1 = A1.allocate(80);

	using ptr = decltype(data1);
	std::pointer_traits<ptr>::pointer pp = data1;
//	double* dp = std::addressof(*data1);
//	double* dp2 = mpi3::shm::pointer_traits<ptr>::to_address(data1);

	if(node.root()){
		raw_reference_cast(data1[3]) = 3.4;
	}
	node.barrier();
	assert( raw_reference_cast(data1[3]) == 3.4);

	A1.deallocate(data1, 80);

	return 0;
}

#endif
#endif

