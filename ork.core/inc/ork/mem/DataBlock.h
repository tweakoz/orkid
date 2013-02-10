#ifndef _ORK_MEM_DATABLOCK_H_
#define _ORK_MEM_DATABLOCK_H_

namespace ork {

class DataBlock
{
public:
	explicit DataBlock(size_t size)
	{
		mDataBlock = static_cast<void *>(new char[size]);
	}
	
	~DataBlock()
	{
		delete [] static_cast<char *>(mDataBlock);
	}
	
	class Allocator
	{
	public:
		Allocator()
			: mAllocationSize(0)
			, mAlignment(4)
		{}
		
		size_t GetAllocationSize() const { return mAllocationSize; }

		size_t GetAlignment() const { return mAlignment; }
		
		size_t Allocate(size_t size, size_t alignment = 4) 
		{
			if(alignment > mAlignment) mAlignment = alignment;
			
			mAllocationSize += (alignment-1); mAllocationSize &= ~(alignment-1);

			size_t result = mAllocationSize;

			mAllocationSize += size;

			return result;
		}
	private:
		size_t mAlignment;
		size_t mAllocationSize;
	};
	
	class IReference
	{
	public:
		virtual size_t Size() const = 0;
		virtual void Construct(DataBlock &) const = 0;
		virtual void Destruct(DataBlock &) const = 0;
		virtual void Allocate(Allocator &) const = 0;
	};
	
	template<typename T>
	class Reference : public IReference
	{
	public:
		Reference() : mOffset(size_t(-1)) {}
		
		/*virtual*/ size_t Size() const { return sizeof(T); }
		
		/*virtual*/ void Allocate(Allocator &proxy) const { if(mOffset == size_t(-1)) mOffset = proxy.Allocate(Size()); }
		/*virtual*/ void Construct(DataBlock &data) const { new(&data[*this]) T(); }
		/*virtual*/ void Destruct(DataBlock &data) const { data[*this].~T(); }
	private:
		friend class DataBlock;
		
		T &Access(void *data) const
		{
			OrkAssertI(mOffset != size_t(-1), "Must call Allocate first");
			return *static_cast<T *>(static_cast<void *>(static_cast<unsigned char *>(data) + mOffset));
		}
		
		const T &Access(const void *data) const
		{
			OrkAssertI(mOffset != size_t(-1), "Must call Allocate first");
			return *static_cast<const T *>(static_cast<const void *>(static_cast<const unsigned char *>(data) + mOffset));
		}
		
		mutable size_t mOffset;
	};
	
	template<typename T>
	inline T& operator[](const DataBlock::Reference<T>& data_reference)
	{
		return data_reference.Access(mDataBlock);
	}
	
	template<typename T>
	inline const T& operator[](const DataBlock::Reference<T>& data_reference) const
	{
		return data_reference.Access(mDataBlock);
	}
	
private:
	void *mDataBlock;
};

class IDataReferenceOperation
{
public:
	virtual void operator()(const DataBlock::IReference &reference) = 0;
};

class AllocateDataReferences : public IDataReferenceOperation
{
	DataBlock::Allocator &mAllocator;
public:
	AllocateDataReferences(DataBlock::Allocator &alloc) : mAllocator(alloc) {}
	
	/*virtual*/ void operator()(const DataBlock::IReference &reference) { reference.Allocate(mAllocator); }
};

class ConstructDataReferences : public IDataReferenceOperation
{
	DataBlock &mBlock;
public:
	ConstructDataReferences(DataBlock &block) : mBlock(block) {}
	
	/*virtual*/ void operator()(const DataBlock::IReference &reference) { reference.Construct(mBlock); }
};

class DestructDataReferences : public IDataReferenceOperation
{
	DataBlock &mBlock;
public:
	DestructDataReferences(DataBlock &block) : mBlock(block) {}
	
	/*virtual*/ void operator()(const DataBlock::IReference &reference) { reference.Destruct(mBlock); }
};

}

#endif
#ifndef _ORK_MEM_DATABLOCK_H_
#define _ORK_MEM_DATABLOCK_H_

namespace ork {

class DataBlock
{
public:
	explicit DataBlock(size_t size)
	{
		mkiDataBlockSize = size;
		mDataBlock = static_cast<void *>(new char[size]);
	}
	
	~DataBlock()
	{
		delete [] static_cast<char *>(mDataBlock);
	}
	
	class Allocator
	{
	public:
		Allocator()
			: mAllocationSize(0)
			, mAlignment(4)
		{}
		
		size_t GetAllocationSize() const { return mAllocationSize; }

		size_t GetAlignment() const { return mAlignment; }
		
		size_t Allocate(size_t size, size_t alignment = 4) 
		{
			if(alignment > mAlignment) mAlignment = alignment;
			
			mAllocationSize += (alignment-1); mAllocationSize &= ~(alignment-1);

			size_t result = mAllocationSize;

			mAllocationSize += size;

			return result;
		}
	private:
		size_t mAlignment;
		size_t mAllocationSize;
	};
	
	class IReference
	{
	public:
		virtual size_t Size() const = 0;
		virtual void Construct(DataBlock &) const = 0;
		virtual void Destruct(DataBlock &) const = 0;
		virtual void Allocate(Allocator &) const = 0;
	};
	
	template<typename T>
	class Reference : public IReference
	{
	public:
		Reference() : mOffset(size_t(-1)) {}
		
		/*virtual*/ size_t Size() const { return sizeof(T); }
		
		/*virtual*/ void Allocate(Allocator &proxy) const { if(mOffset == size_t(-1)) mOffset = proxy.Allocate(Size()); }
		/*virtual*/ void Construct(DataBlock &data) const { new(&data[*this]) T(); }
		/*virtual*/ void Destruct(DataBlock &data) const { data[*this].~T(); }
	private:
		friend class DataBlock;
		
		T &Access(DataBlock *db) const
		{
			OrkAssert( mOffset+sizeof(T) < db->mkiDataBlockSize );
			return *static_cast<T *>(static_cast<void *>(static_cast<unsigned char *>(data) + mOffset));
		}
		
		const T &Access(const DataBlock *db) const
		{
			OrkAssert( mOffset+sizeof(T) < db->mkiDataBlockSize );
			return *static_cast<const T *>(static_cast<const void *>(static_cast<const unsigned char *>(data) + mOffset));
		}
		
		mutable size_t mOffset;
	};
	
	template<typename T>
	inline T& operator[](const DataBlock::Reference<T>& data_reference)
	{
		return data_reference.Access(this);
	}
	
	template<typename T>
	inline const T& operator[](const DataBlock::Reference<T>& data_reference) const
	{
		return data_reference.Access(this);
	}
	
private:
	void *mDataBlock;
	int	mkiDataBlockSize;
};

class IDataReferenceOperation
{
public:
	virtual void operator()(const DataBlock::IReference &reference) = 0;
};

class AllocateDataReferences : public IDataReferenceOperation
{
	DataBlock::Allocator &mAllocator;
public:
	AllocateDataReferences(DataBlock::Allocator &alloc) : mAllocator(alloc) {}
	
	/*virtual*/ void operator()(const DataBlock::IReference &reference) { reference.Allocate(mAllocator); }
};

class ConstructDataReferences : public IDataReferenceOperation
{
	DataBlock &mBlock;
public:
	ConstructDataReferences(DataBlock &block) : mBlock(block) {}
	
	/*virtual*/ void operator()(const DataBlock::IReference &reference) { reference.Construct(mBlock); }
};

class DestructDataReferences : public IDataReferenceOperation
{
	DataBlock &mBlock;
public:
	DestructDataReferences(DataBlock &block) : mBlock(block) {}
	
	/*virtual*/ void operator()(const DataBlock::IReference &reference) { reference.Destruct(mBlock); }
};

}

#endif
