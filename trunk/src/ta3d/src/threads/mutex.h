/*  TA3D, a remake of Total Annihilation
	Copyright (C) 2005  Roland BROCHARD

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA*/

#ifndef __TA3D_THREADS_MUTEX_H__
# define __TA3D_THREADS_MUTEX_H__

# include <yuni/thread/mutex.h>
# include <yuni/thread/condition.h>


namespace TA3D
{


	typedef Yuni::Mutex  Mutex;


	/*! \class MutexLocker
	**
	** \code
	**      class Foo
	**      {
	**      public:
	**          Foo() : pValue(42) {}
	**          ~Foo() {}
	**          int getValue()
	**          {
	**              MutexLocker locker(pMutex);
	**              return pValue;
	**          }
	**          void setValue(const int i)
	**          {
	**              pMutex.lock();
	**              pValue = i;
	**              pMutex.unlock();
	**          }
	**      private:
	**          int pValue;
	**          Mutex pMutex;
	**      };
	** \endcode
	*/
	class MutexLocker
	{
	public:
		MutexLocker(Mutex& m) : pMutex(m) { m.lock(); }
		~MutexLocker() { pMutex.unlock(); }
	private:
		Mutex& pMutex;

	}; // MutexLocker


	class Synchronizer : public Yuni::Thread::Condition
	{
	public:
		Synchronizer(int nbThreadsToSync);
		~Synchronizer();

		void sync();
		void release();

		int getNbWaitingThreads()	{	return nbThreadsWaiting;	}
		void setNbThreadsToSync(int n)	{	nbThreadsToSync = n;	}

	private:
		int nbThreadsToSync;
		int nbThreadsWaiting;
	};

} // namespace TA3D


#endif // __TA3D_THREADS_MUTEX_H__
