/*实现智能指针 (Custom unique_ptr / shared_ptr)
考察点：RAII 机制、拷贝构造/赋值运算符的显式禁用、引用计数与线程安全。*/

the following code is my written code. 

template <typename T>
class shared_ptr{
    T *m_ptr;
    std::atomic<int>*m_count;

    shared_ptr(const shared_ptr& other)
   {
      m_ptr= other.m_ptr;
      m_count = other.m_count;
      if(m_count) (*m_count)++;
   }
   shared_ptr operator=(const shared_ptr& other){
      if(this!=other){
        release();
        m_ptr= other.m_ptr;
        m_count = other.m_count;
        if(m_count) (*m_count)++;
      }
      return *this
   }
   void release(){
    (*m_count)--
    if(m_count==0){
      delete m_ptr;
      delete m_count;
    }
  }
   ~shared_ptr(){
     release();
  }
}
