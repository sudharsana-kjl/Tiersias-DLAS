#include <iostream>
#include <algorithm>
#include <vector>
#include <fstream>
#include <cmath>
#include <set>
#include <string>
#include <sstream>
#include <unordered_map>
#include <map>
using namespace std;



// JOB STATUS
// 0 END 
// 2 ADDED 
// 3 PENDING 
// 4 EVENT 
// 5 RUNNING 

int worker_mem = 5;
int ps_mem = 8;
float per_worker_mem = 0.2;
unordered_map<string,int> model_name_index_map;
int job_start_queue_index = 0;
unordered_map<int, vector<int > > start_job_dicts, end_job_dicts;
int job_end_queue_index = 0;


typedef struct Node{
	int id;
	int num_cpu;
	int num_gpu;
	int mem;
	int free_mem;
	int free_cpus;
	int free_gpus;

	Node(int i, int n_gpu, int n_cpu, int me){
		id = i;
		num_cpu = n_cpu;
		free_cpus = n_cpu;
		num_gpu = n_gpu;
		free_gpus = n_gpu;
		mem = me;
		free_mem = me;
		cout<<"Node "<<id<<" initialised with "<<num_gpu<<" GPU "<<num_cpu<<" CPU "<<mem<<"G memory"<<endl;
	}

	void init_node(int n_gpu=0, int n_cpu=0, int me=0){
		num_cpu = n_cpu;
		free_cpus = n_cpu;
		num_gpu = n_gpu;
		free_gpus = n_gpu;
		mem = me;
		free_mem = me;
	}
}Node;

typedef struct Switch{
	int id;
	int num_node;
	int num_gpu_node;
	int num_cpu_node;
	int mem_node;
	vector<	Node*> node_list;

	Switch(int i, int n_node, int n_gpu_node, int n_cpu_node, int m_node){
		id = i;
		num_node = n_node;
		num_gpu_node = n_gpu_node;
		num_cpu_node = n_cpu_node;
		mem_node = m_node;
		cout<<"Switch "<<id<<" initialised with "<<num_node<<" Nodes\n";
	}

	void add_nodes(int n_node, int n_gpu_node, int n_cpu_node, int m_node){
		num_node = n_node;
		num_gpu_node = n_gpu_node;
		num_cpu_node = n_cpu_node;
		mem_node = m_node;
		for(int i=0;i<n_node;i++){
			Node* tmp = new Node(i,n_gpu_node,n_cpu_node,m_node);
			node_list.push_back(tmp);
		}
	}


}Switch;


typedef struct Jobs{
	int num_job = 0;        
    vector<map<string,int> >job_list;
	vector<map<string,int> >job_events;     
    vector<int >runnable_jobs;
    int num_queue = 3;
    vector< vector< int > > queues;
    int queue_limit[3] = {3250, 7200, 18000};
	int worker_mem = 5;
	int ps_mem = 6;

	void add_job(map<string,int> job_dict){
		job_dict["duration"] = (int) job_dict["duration"];
		if(job_dict.find("start_time") == job_dict.end())
			job_dict["start_time"] = 0;
		if(job_dict.find("end_time") == job_dict.end())
			job_dict["end_time"] = 0;
		if(job_dict.find("pending_time") == job_dict.end())
			job_dict["pending_time"] = 0;

		job_dict["start_time"] = INT_MAX;
        job_dict["end_time"] = 0;
        job_dict["pending_time"] = 0;

        job_dict["execution_time"] = 0;
        job_dict["last_start_time"] = 0;
        job_dict["last_check_time"] = 0;
        job_dict["executed_time"] = 0;

        job_dict["preempt"] = 0;
        job_dict["resume"] = 0;
        job_dict["promote"] = 0;

        job_dict["status"] = 2; //ADDED
        job_dict["job_idx"] = job_list.size();

        job_list.push_back(job_dict);
        num_job += 1;
	}

	static bool compareJobs(map<string,int> map1, map<string,int> map2){
		return map1["submit_time"] < map2["submit_time"];
	}

	static bool compareJobsTime(map<string,int> map1, map<string,int> map2){
		return map1["time"] < map2["time"];
	}

	void sort_all_jobs(){
		sort(job_list.begin(), job_list.end(), compareJobs);
		cout<<"Sorted jobs with their start time";
	}

	void move_to_runnable(int job_ind){
		job_list[job_ind]["status"] = 3; // "PENDING"
        job_list[job_ind]["start_time"] = INT_MAX;
        job_list[job_ind]["last_start_time"] = 0;
        job_list[job_ind]["last_check_time"] = job_list[job_ind]["submit_time"];
        job_list[job_ind]["total_executed_time"] = 0;
        job_list[job_ind]["executed_time"] = 0; 
        job_list[job_ind]["pending_time"] = 0;
        job_list[job_ind]["last_pending_time"] = 0; 
        runnable_jobs.push_back(job_list[job_ind]["job_idx"]);
	}

	void prepare_job_start_events(){
		for(auto job: job_list){
			int start_t = job["submit_time"];
			map<string,int> tmp_dict;
			for(auto e : job_events){
				if(e["time"] == start_t)
					tmp_dict = e;
			}
			if(tmp_dict.size()==0){
				tmp_dict["time"] = start_t;
                tmp_dict["start_jobs"] = job_start_queue_index;
                start_job_dicts[job_start_queue_index].push_back(job["job_idx"]);
                job_start_queue_index++; 
                job_events.push_back(tmp_dict);
			} else {
				start_job_dicts[tmp_dict["start_jobs"]].push_back(job["job_idx"]);
			}

			 job["status"] = 4; // "EVENT" 
		}

		sort(job_events.begin(), job_events.end(), compareJobsTime);
		cout<<"Initialize and Add job start events\n";
		
		for(auto event : job_events){
			cout<<"Job Event time : "<<event["time"]<<" Started jobs : "<<start_job_dicts[event["start_jobs"]
				].size()<<" End jobs : "<<end_job_dicts[event["end_jobs"]].size()<<endl;

		}
	}

}Jobs;

Jobs jobs;

typedef struct Cluster{
	int num_switch;
	int num_node_switch;
	int num_gpu_pnode;
	int num_cpu_pnode;
	int mem_pnode;
	int num_node;
	int free_gpu;

	int num_gpu;
	int num_cpu;
	int mem;
	vector<Switch*> switch_list;

	void set_spec(int n_switch, int n_node_switch, int n_gpu_node, int n_cpu_node, int m_node){
		num_switch = n_switch;
		num_node_switch = n_node_switch;
		num_gpu_pnode = n_gpu_node;
		num_cpu_pnode =  n_cpu_node;
		mem_pnode = m_node;
		num_node = n_switch*n_node_switch;
		num_gpu = num_node*num_gpu_pnode;
		num_cpu = num_node*num_cpu_pnode;
		mem = num_node*mem_pnode;
	}

	void init_infra(int n_switch, int n_node_switch, int n_gpu_node, int n_cpu_node, int m_node){
		set_spec(n_switch, n_node_switch, n_gpu_node, n_cpu_node, m_node);
		for(int s=0;s<n_switch;s++){
			Switch* tmp = new Switch(s, n_node_switch, n_gpu_node, n_cpu_node, m_node);
			tmp->add_nodes(n_node_switch,n_gpu_node,n_cpu_node,m_node);
			switch_list.push_back(tmp);
		}

		cout<<"Cluster is ready with the below spec\n";
		cout<<"Switch: "<<n_switch<<" Node : "<<num_node<<endl;
	}

	void empty_infra(){
		free_gpu = num_gpu;
		for(auto sw : switch_list){
			for(auto node : (*sw).node_list){
				(*node).init_node(num_gpu_pnode,num_cpu_pnode);
			}
		}
	}


	bool release_job_res(int job_ind){
		free_gpu += jobs.job_list[job_ind]["num_gpu"];
		if(free_gpu > num_gpu)
			free_gpu = num_gpu;
		jobs.job_list[job_ind]["status"] = 0; //END
		cout<<"Job Completed : "<<jobs.job_list[job_ind]["job_idx"]<<endl;
		return true;

	}

}Cluster;

Cluster cluster;


void job_file_parser(string trace_file){
	ifstream input_file;
	string line;
	input_file.open(trace_file);

	if (!input_file.is_open()) {
		cerr << "Unable to Open the file : "<< trace_file <<endl;
		return;
	}
	string headers;
	getline(input_file,headers,'\n');
	vector<string> header_list;
	stringstream ss(headers);
	
	while (ss.good()) {
        string substr;
        getline(ss, substr, ',');
        header_list.push_back(substr);
    }

    cout<<"Below fields are camptured from the job trace file \n";
    for(string i : header_list){
    	cout<<i<<" ";
    }
    cout<<endl;
    int c=0;
    while(getline(input_file, line, '\n')){
    	map<string, int> m;
    	vector<string> row_list;
		stringstream ss(line);
		
		while (ss.good()) {
	        string substr;
	        getline(ss, substr, ',');
	        row_list.push_back(substr);
	    }
	    for(int i=0;i<header_list.size()-1;i++){
	    	if(model_name_index_map.find(row_list[i])!= model_name_index_map.end()){
	    		m[header_list[i]] = model_name_index_map[row_list[i]];
	    	}
	    	else m[header_list[i]] = stoi(row_list[i]);
	    }
    	jobs.add_job(m);
    	c++;
    }
    jobs.sort_all_jobs();
    cout<<"Total jobs received : "<<c<<endl;
    input_file.close();
}


void parse_cluster_spec(string spec_file){
	ifstream input_file;
	string line;
	input_file.open(spec_file);
	if (!input_file.is_open()) {
		cerr << "Unable to Open the file : ""<< spec_file << """ << endl;
		return ;
	}
	string headers;
	getline(input_file,headers,'\n');
	vector<string> header_list;
	stringstream ss(headers);
	
	while (ss.good()) {
        string substr;
        getline(ss, substr, ',');
        header_list.push_back(substr);
    }

    getline(input_file,line,'\n');
    vector<int> row_list;
	stringstream sss(line);
	
	while (sss.good()) {
        string substr;
        getline(sss, substr, ',');
        row_list.push_back(stoi(substr));
    }

    int num_switch, num_node_p_switch, num_gpu_p_node,num_cpu_p_node,mem_p_node;
    for(int i=0;i<header_list.size();i++){
    	if(header_list[i]=="num_switch")
    		num_switch = row_list[i];
    	else if(header_list[i]=="num_node_p_switch")
    		num_node_p_switch = (int)row_list[i];
    	else if(header_list[i]=="num_gpu_p_node")
    		num_gpu_p_node = (int)row_list[i];
    	else if(header_list[i]=="num_cpu_p_node")
    		num_cpu_p_node = (int)row_list[i];
    	else if(header_list[i]=="mem_p_node")
    		mem_p_node = (int)row_list[i];

    }
    cout<<"No of Switch : "<<num_switch<<" Node_p_switch : "<< num_node_p_switch<<endl;
    cout<<"No of GPU node : "<<num_gpu_p_node<<" CPU node : "<<num_cpu_p_node<<" Memory : "<<mem_p_node<<"G \n";
    cluster.init_infra(num_switch, num_node_p_switch,num_gpu_p_node,num_cpu_p_node,mem_p_node);

    input_file.close();
}


void dlas(){

	vector<map<string,int> >end_events;
	int next_job_jump = INT_MAX;

	while(jobs.job_events.size() + jobs.runnable_jobs.size() > 0){
		if(jobs.job_events.size() + end_events.size() == 0){
			break;
		}
		map<string,int> start_event,end_event,event;
		int start_time = INT_MAX;
		int end_time = INT_MAX;
		int event_time = INT_MAX;

		if(jobs.job_events.size() > 0){
			start_event = jobs.job_events[0];
			start_time = start_event["time"];
		}
		if(end_events.size() > 0){

			end_event = end_events[0];
			end_time = end_event["time"];
		}
		event["time"] = INT_MAX;
		if(end_time < start_time){
			event_time = end_time;
			event = end_event;
		}
		else if(end_time > start_time){
			event_time = start_time;
			event = start_event;
		}
		else if((end_time == start_time) && (end_time!=INT_MAX)){
			event_time = start_time;
			event = start_event;
			event["end_jobs"] = end_events[0]["end_jobs"];
		}
		if(event_time > next_job_jump){
			event_time = next_job_jump;
			event.clear();
		}

		if(event.find("end_jobs")!= event.end()){
			for(int e_job : end_job_dicts[event["end_jobs"]]){ 
				cluster.release_job_res(e_job);

				vector<int>::iterator rmval = find(jobs.queues[jobs.job_list[e_job]["q_id"]].begin(), jobs.queues[jobs.job_list[e_job]["q_id"]].end(),e_job);
				if(rmval != jobs.queues[jobs.job_list[e_job]["q_id"]].end())
					jobs.queues[jobs.job_list[e_job]["q_id"]].erase(rmval);
			}
		}
		if(event.find("start_jobs")!= event.end()){
			for(auto s_job : start_job_dicts[event["start_jobs"]]){
				jobs.move_to_runnable(s_job);
				jobs.job_list[s_job]["q_id"]=0;
				if(jobs.queues.size() == 0){
					vector<int > vec;
					vec.push_back(s_job);
					jobs.queues.push_back(vec);
				}
				else
					((jobs.queues)[0]).push_back(s_job);
				cout<<"Job added : "<<jobs.job_list[s_job]["job_idx"]<<endl;
			}
			auto index = jobs.job_events.begin();
			jobs.job_events.erase(index); 
		}
		for(auto rjob : jobs.runnable_jobs){
			if( 5 == jobs.job_list[rjob]["status"]){ //"RUNNING" 
				int tmp = event_time - jobs.job_list[rjob]["last_check_time"];
				jobs.job_list[rjob]["total_executed_time"] = jobs.job_list[rjob]["total_executed_time"] + tmp;
                jobs.job_list[rjob]["executed_time"] = jobs.job_list[rjob]["executed_time"] + tmp;
                jobs.job_list[rjob]["last_check_time"] = event_time;
                int j_gt = jobs.job_list[rjob]["executed_time"];
                int cur_qid = jobs.job_list[rjob]["q_id"];
                if(cur_qid < jobs.num_queue - 1){
                    if(j_gt >= jobs.queue_limit[cur_qid]){
                        jobs.job_list[rjob]["q_id"] = cur_qid + 1;
                        jobs.queues[jobs.job_list[rjob]["q_id"]].push_back(rjob);
                        for(auto i = jobs.queues[cur_qid].begin(); i!= jobs.queues[cur_qid].end(); i++){
		        			if(rjob==(*i)["job_idx"])
		        				jobs.queues[cur_qid].erase(i);
			        	}
                        cout<<"job "<<jobs.job_list[rjob]["job_idx"]<<" demote to Q "<<jobs.job_list[rjob]["q_id"]<<endl;
                    }
                }

			}
			else if( 3 == jobs.job_list[rjob]["status"]){
				int tmp = event_time - jobs.job_list[rjob]["last_check_time"];
                jobs.job_list[rjob]["last_check_time"] = event_time;
                jobs.job_list[rjob]["pending_time"] = rjob["pending_time"] + tmp; 
                if(jobs.job_list[rjob]["executed_time"] > 0) 
                    jobs.job_list[rjob]["last_pending_time"] = jobs.job_list[rjob]["last_pending_time"] + tmp; 
			}
		}

		cluster.empty_infra();
        vector<int >run_jobs, preempt_jobs;

        for(auto queue: jobs.queues){
        	for(auto job: queue){
        		if(cluster.free_gpu >= jobs.job_list[job]["num_gpu"]){
        			if(jobs.job_list[job]["status"] == 3)
        				run_jobs.push_back(job);
        			cluster.free_gpu = cluster.free_gpu - jobs.job_list[job]["num_gpu"];
        		}
        		else {
        			if(jobs.job_list[job]["status"] == 5)
        				preempt_jobs.push_back(job);
        			continue;
        		}
        	}
        }

        for(auto job : preempt_jobs){
        	jobs.job_list[job]["status"] = 3;
        	jobs.job_list[job]["preempt"] = jobs.job_list[job]["preempt"]+1;
        }
        for(auto job: run_jobs){
        	jobs.job_list[job]["status"] = 5;
        	jobs.job_list[job]["resume"] = jobs.job_list[job]["resume"]+1;
        	if(jobs.job_list[job]["start_time"] == INT_MAX)
        		jobs.job_list[job]["start_time"] = event_time;
        }

        for(auto queue : jobs.queues){
        	vector<int>pending_job;
        	for(auto job: queue){
        		if(jobs.job_list[job]["status"] == 3)
        			pending_job.push_back(job);
        	}
        	for(auto i = queue.begin(); i!= queue.end(); i++){
        		for(auto j = pending_job.begin(); j!= pending_job.end();j++){
        			if((*j)==(*i))
        				queue.erase(i);
        		}
        	}
        	queue.insert(queue.end(), pending_job.begin(), pending_job.end());
        }
        end_events.clear();
        int min_end_time = INT_MAX;
        map<string,int> tmp_end_event;
        for(auto rjob: jobs.runnable_jobs){
        	if(jobs.job_list[rjob]["status"] == 5){
        		int remaining_time = jobs.job_list[rjob]["duration"] - jobs.job_list[rjob]["total_executed_time"];
        		end_time = event_time + remaining_time;
        		if(end_time < min_end_time){
        			tmp_end_event["time"] = end_time;
                    tmp_end_event["end_jobs"] = job_end_queue_index;
                    end_job_dicts[tmp_end_event["end_jobs"]].push_back(rjob);
                    job_end_queue_index++;
                    min_end_time = end_time;
        		}
        		else if(min_end_time == end_time)
        			end_job_dicts[tmp_end_event["end_jobs"]].push_back(rjob);
        	}
        }

        if(min_end_time < INT_MAX){
        	end_events.push_back(tmp_end_event);
        }

        next_job_jump = INT_MAX;
        for(auto rjob: jobs.runnable_jobs){
        	if(jobs.job_list[rjob]["status"] == 5){
        		int qid = jobs.job_list[rjob]["q_id"];
        		if(qid < jobs.num_queue -1){
        			int jump_time = jobs.queue_limit[qid] - jobs.job_list[rjob]["executed_time"] + event_time;
        			if(jump_time < next_job_jump)
        				next_job_jump = jump_time;
        		}
        	}
        }
	}
}

int main(){
	model_name_index_map["vgg19"] = 0;
	model_name_index_map["vgg16"] = 1;
	model_name_index_map["vgg11"] = 2;
	model_name_index_map["alexnet"] = 3;
	model_name_index_map["resnet152"] = 4;
	model_name_index_map["resnet101"] = 5;
	model_name_index_map["resnet50"] = 6;
	model_name_index_map["inception4"] = 7;
	model_name_index_map["inception3"] = 8;
	model_name_index_map["googlenet"] = 9;
	job_file_parser("job_trace_file.csv");
    parse_cluster_spec("cluster_spec.csv");
    jobs.prepare_job_start_events();
    dlas();
}

