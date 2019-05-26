
#ifndef WORDSCOUNTERSECOND_PARALLELQUEUE_H
#define WORDSCOUNTERSECOND_PARALLELQUEUE_H


template <typename T>
class QueueNode{
private:
    T data;
    QueueNode *pntr = nullptr;
public:
    QueueNode(){};
    QueueNode(T dat){data = dat;};
    QueueNode* getLink(){return pntr;}
    void setValue(T value){data = value;}
    void setLink(QueueNode *link) { pntr = link;}
    T getValue(){return data;}
};

template <typename T>
class ParallelQueue {
private:
    QueueNode<T> *head = nullptr;
    QueueNode<T> *tail = nullptr;
    int len = 0;
public:
    bool isEmpty(){
        return head==nullptr;
    }
    void enque(T val){
        if (head != nullptr){
            auto *pointer = new QueueNode<T>(val);
            tail->setLink(pointer);
            tail = pointer;
        }else{
            auto *new_node = new QueueNode<T>(val);
            head = new_node;
            tail = new_node;
        }
        len++;
    };
    T deque(){
        auto old_head = head->getValue();
        auto *new_head = head->getLink();
        delete head;
        head = new_head;
        len--;
        return old_head;
    };
    void printContent(){
        auto iterator = head;
        while(iterator != nullptr){
            std::cout << iterator->getValue();
            iterator = iterator->getLink();
        }
    };
    void printSize(){
      std::cout<<len<<std::endl;
    };
};

#endif //WORDSCOUNTERSECOND_PARALLELQUEUE_H
