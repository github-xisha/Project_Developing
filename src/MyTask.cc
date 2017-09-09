 ///
 /// @file    MyTask.cc
 /// @author  lemon(haohb13@gmail.com)
 /// @date    2016-03-28 15:34:24
 ///

#include "MyTask.h"
#include "MyDict.h"
#include "CacheManager.h"
#include "EditDistance.h"
#include "Thread.h"
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <sstream>
using std::cout;
using std::endl;
using std::istringstream;


MyTask::MyTask(const string & queryWord, const wd::TcpConnectionPtr & conn)
: _queryWord(queryWord)
, _conn(conn)
{
}

void MyTask::execute()
{
	//1. 从cache里面进行查找
	Cache & cache = CacheManager::getCache(wd::str2int(wd::current_thread::threadName));
	string result = cache.query(_queryWord);
	if(result != string())
	{
		_conn->sendInLoop(result);
		cout << "> response client" << endl;
		return;//如果在Cache之中找到了，就不需要进行后面的查询了
	}

	queryIndexTable();//2. 执行查询,查询索引表

	response();//3. 给客户端返回结果
}


void MyTask::queryIndexTable()
{
	auto indexTable = MyDict::createInstance()->get_index_table();
	string ch;

	for(int idx = 0; idx != _queryWord.size();)
	{
		size_t nBytes = nBytesCode(_queryWord[idx]);
		ch = _queryWord.substr(idx, nBytes);
		idx += nBytes;
		if(indexTable.count(ch))
		{
			cout << "indexTable has character " << ch << endl;
			statistic(indexTable[ch]);
		}
	}
}

void MyTask::response()
{
	if(_resultQue.empty())
	{
		string result = "no answer!";
		_conn->sendInLoop(result);
	}
	else
	{
		istringstream ist;
       // MyResult result = _resultQue.top();
        // _resultQue.pop();
       // MyResult result1 = _resultQue.top();

        MyResult result[3];
        //std::cout<<"优先级队列元素个数："<<_resultQue.size()<<std::endl;
        for(int i=0;i<3;++i)
        {
            result[i] = _resultQue.top();
            _resultQue.pop();
            if(_resultQue.empty())//每次从队列弹出一个元素后，需判断队列是否为空
            {
                for(int j=i;j<3;++j)
                {
                    result[j]=result[0];
                }
                break;
            }
        }
        string line("---");
        string sendData=result[0]._word+line+result[1]._word+line+result[2]._word;
        _conn->sendInLoop(sendData);//返回3个候选词
        //_conn->sendInLoop(result._word);//这里只返回了一个候选词

		Cache & cache = CacheManager::getCache(wd::str2int(wd::current_thread::threadName));
		//cache.addElement(_queryWord, result[0]._word);//在缓存中添加新的查询结果
		cache.addElement(_queryWord,sendData); //在缓存中添加3个查询结果
        cout << "> respone(): add Cache" << endl;
	}
	cout << "> reponse client" << endl;
}

void MyTask::statistic(set<int> & iset)
{
	auto dict = MyDict::createInstance()->get_dict();
	auto iter = iset.begin();
	for(; iter != iset.end(); ++iter)
	{
		string rhsWord = dict[*iter].first;
		int idist = distance(rhsWord);//进行最小编辑距离的计算
		if(idist < 3)
		{
			MyResult res;
			res._word = rhsWord;
			res._iFreq = dict[*iter].second;
			res._iDist = idist;
			_resultQue.push(res);
		}
	}
}

int MyTask::distance(const string & rhsWord)
{
	return ::editDistance(_queryWord, rhsWord);
}
