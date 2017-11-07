#ifndef _NODE_H_
#define _NODE_H_

//////////////////////////////////////////////////////////////////////////////

template<class TYPE> class Node {
	Node *_prev, *_next;

public:
	Node()
	{ _prev = _next = this; }

	Node(const Node&)
	{ _prev = _next = this; }

	Node& operator=(const Node&)
	{ return *this; }

	TYPE *GetPrev() const
	{ return static_cast<TYPE *>(_prev); }

	TYPE *GetNext() const
	{ return static_cast<TYPE *>(_next); }

	bool IsEmpty() const
	{ return _prev == this; }

	void LinkNext(Node *head) {
		Node *tail = head->_prev;
		Node *next = this->_next;
		this->_next = head;
		head->_prev = this;
		tail->_next = next;
		next->_prev = tail;
	}

	void LinkPrev(Node *tail) {
		Node *head = tail->_next;
		Node *prev = this->_prev;
		this->_prev = tail;
		tail->_next = this;
		head->_prev = prev;
		prev->_next = head;
	}

	void Unlink() {
		_prev->_next = _next;
		_next->_prev = _prev;
		_prev = _next = this;
	}
};

//////////////////////////////////////////////////////////////////////////////

#endif // _NODE_H_
