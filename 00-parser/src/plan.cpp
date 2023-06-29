#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cassert>
#include "parsetree.hpp"
#include "util.hpp"
#include "plan.hpp"

using namespace std;


void print_plan(parsed_plan converted_plan, ostream & pout){
	// write the plan to the output
	pout << "==>" << endl;
	for (int action_id : converted_plan.primitive_plan){
		instantiated_plan_step ps = converted_plan.tasks[action_id];
		pout << action_id << " " << ps.name;
		for (string arg : ps.arguments) pout << " " << arg;
		pout << endl;
			
	}
	
	pout << "root";
	for (int root : converted_plan.root_tasks) pout << " " << root;
	pout << endl;


	for (auto task : converted_plan.tasks){
		if (task.second.declaredPrimitive) continue;

		instantiated_plan_step ps = task.second;
		pout << task.first << " " << ps.name;
		for (string arg : ps.arguments) pout << " " << arg;
		
		pout << " -> " << converted_plan.appliedMethod[task.first];
		for (int subtask : converted_plan.subtasksForTask[task.first]) if (subtask >= 0) pout << " " << subtask;
		pout << endl;
	}

}
vector<string> parse_list_of_strings(istringstream & ss, int debugMode){
	vector<string> strings;
	while (true){
		if (ss.eof()) break;
		string x; ss >> x;
		strings.push_back(x);
	}

	return strings;
}

vector<string> parse_list_of_strings(string & line, int debugMode){
	if (debugMode) cout << "Reading list of strings from \"" << line << "\"" << endl;
	istringstream ss (line);
	return parse_list_of_strings(ss,debugMode);
}

vector<int> parse_list_of_integers(istringstream & ss, int debugMode){
	vector<int> ints;
	while (true){
		if (ss.eof()) break;
		string xx; ss >> xx;
		char* cstring;
		long x = strtol(xx.c_str(),&cstring,10);
		if (*cstring) {
			if (debugMode)
				cerr << "Expected integer but found \""	<< xx << "\" .. I'm going to try to ignore" << endl;
			continue;
		}
		ints.push_back(x);
	}

	return ints;
}

vector<int> parse_list_of_integers(string & line, int debugMode){
	if (debugMode) cout << "Reading list of integers from \"" << line << "\"" << endl;
	line.erase(0,line.find_first_not_of(" "));
	line.erase(line.find_last_not_of(" ") + 1);
	if (!line.size()) return vector<int>();
	istringstream ss (line);
	return parse_list_of_integers(ss,debugMode);
}

pair<string,vector<string>> parse_task_with_arguments_from_braced_expression(string str){
	string task = "";
	size_t pos= 0;
	while (pos < str.size() && str[pos] != '['){
   		task += str[pos];
		pos++;
	}
	
	vector<string> task_arguments;

   	if (pos != str.size()){
		string argument_string = str.substr(pos+1,str.size() - pos - 2);
		replace(argument_string.begin(), argument_string.end(), ',', ' ');
		task_arguments = parse_list_of_strings(argument_string,0);
	} // else there are none

	return make_pair(task,task_arguments);
}

instantiated_plan_step parse_plan_step_from_string(string input, int debugMode){
	if (debugMode) cout << "Parse instantiated task from \"" << input << "\" ... ";
	
	// removed braces for convenience
	replace(input.begin(), input.end(), '(', ' ');
	replace(input.begin(), input.end(), ')', ' ');

	istringstream ss (input);
	bool first = true;
	instantiated_plan_step ps;
	while (1){
		if (ss.eof()) break;
		string s; ss >> s;
		if (s == "") break;
		if (first) {
			first = false;
			// allow for braced arguments of tasks
			auto [name,braced_arguments] = parse_task_with_arguments_from_braced_expression(s);
			ps.name = name;
			ps.arguments = braced_arguments;
		} else {
			ps.arguments.push_back(s);
		}
	}
	if (debugMode) cout << "done" << endl;
	return ps;
}



parsed_plan parse_plan(istream & plan, int debugMode){
	parsed_plan pplan;
	
	// parse everything until marker
	string s = "";
	while (s != "==>") {
		plan >> s;
		if (plan.eof()) return pplan;
	}
	// then read the primitive plan


	pplan.tasks.clear();
	pplan.primitive_plan.clear();
	pplan.pos_in_primitive_plan.clear();
	pplan.appliedMethod.clear();
	pplan.subtasksForTask.clear();
	pplan.root_tasks.clear();
	

	if (debugMode) cout << "Reading plan given as input" << endl;
	bool planAlreadyEnded = false;
	while (1){
		string head; plan >> head;
		if (head == "root") break;
		if (head == "<=="){
			planAlreadyEnded = true;
			break;
		}
		int id = atoi(head.c_str());
		if (id < 0){
			cout << color(COLOR_RED,"Negative id: ") << color(COLOR_RED,to_string(id)) << endl;
			exit(1);
		}
		string rest_of_line; getline(plan,rest_of_line);
		instantiated_plan_step ps = parse_plan_step_from_string(rest_of_line, debugMode);		
		ps.declaredPrimitive = true;
		pplan.primitive_plan.push_back(id);
		pplan.pos_in_primitive_plan[id] = pplan.primitive_plan.size() - 1;
		if (pplan.tasks.count(id)){
			cout << color(COLOR_RED,"Two primitive task have the same id: ") <<	color(COLOR_RED,to_string(id)) << endl;
			exit(1);
		}
		pplan.tasks[id] = ps;
		if (debugMode) {
			cout << "Parsed action id=" << id << " " << ps.name;
			for(string arg : ps.arguments) cout << " " << arg;
			cout << endl;
		}
	}


	if (debugMode) cout << "Size of primitive plan: " << pplan.primitive_plan.size() << endl;
	string root_line = ""; if (!planAlreadyEnded) getline(plan,root_line);
	pplan.root_tasks = parse_list_of_integers(root_line, debugMode);
	if (debugMode) {
		cout << "Root tasks (" << pplan.root_tasks .size() << "):";
		for (int & rt : pplan.root_tasks) cout << " " << rt;
		cout << endl;
	}


	if (debugMode) cout << "Reading plan given as input" << endl;
	if (!planAlreadyEnded) while (1){
		string line;
		getline(plan,line);
		if (plan.eof()) {
			if (debugMode) cout << "Reached end of input." << endl;
			break;
		}
	    size_t first = line.find_first_not_of(' ');
    	size_t last = line.find_last_not_of(' ');
		if (first == string::npos){
			if (debugMode) cout << " ... empty line." << endl;
			continue;
		}
		line = line.substr(first, (last-first+1));
		
		istringstream ss (line);
		string id_string; ss >> id_string;
		if (id_string == "<=="){
			if (debugMode) cout << "Reached end of plan (marked)." << endl;
			break;
		}

		int id = stoi(id_string);
		if (ss.fail()){
			if (debugMode) cout << "Reached end of plan." << endl;
			break;
		}

		if (id < 0){
			cout << color(COLOR_RED,"Negative id: ") << color(COLOR_RED,to_string(id)) << endl;
			exit(1);
		}
		string task = "";
		s = "";
		do {
			task += " " + s;
			ss >> s;
		} while (s != "->");
		instantiated_plan_step at = parse_plan_step_from_string(task, debugMode);
		at.declaredPrimitive = false;
		// add this task to the map
		if (pplan.tasks.count(id)){
			cout << color(COLOR_RED,"Two task have the same id: ") << color(COLOR_RED,to_string(id)) << endl;
			exit(1);
		}
		pplan.tasks[id] = at;
		if (debugMode) {
			cout << "Parsed abstract task id=" << id << " " << at.name;
			for(string arg : at.arguments) cout << " " << arg;
			cout << endl;
		}
		

		// read the actual content of the method
		string methodName; ss >> methodName;
		if (debugMode) {
			cout << "Parsed method name: " << methodName << endl;
		}
		// read subtask IDs
		vector<int> subtasks = parse_list_of_integers(ss, debugMode);
		
		// id cannot be contained in the maps as it was possible to insert the id into the tasks map
		pplan.appliedMethod[id] = methodName;
		pplan.subtasksForTask[id] = subtasks;

		if (debugMode) {
			cout << "Subtasks:";
			for(int st : subtasks) cout << " " << st;
			cout << endl << endl;
		}
	}

	return pplan;
}


int get_next_free_id(parsed_plan & plan){
	int max_id = -1;
	for (auto task : plan.tasks)
		max_id = max(max_id, task.first);
	return max_id + 1;
}


void update_index(parsed_plan & plan){
	plan.task_contained_in.clear();
	// find the rule where expanded is contained
	for (auto sub : plan.subtasksForTask)
		for (int subtask : sub.second)
			plan.task_contained_in[subtask] = sub.first;
}


parsed_plan expand_compressed_action(parsed_plan & plan, int expanded_task){
	instantiated_plan_step expanded_ps = plan.tasks[expanded_task];
	string task_name = expanded_ps.name;

	vector<string> blocks; blocks.push_back("");
	for (size_t i = 1; i < task_name.size()-1; i++){
		if (task_name[i] == '#'){
			blocks.push_back("");
			continue;
		}
		blocks[blocks.size()-1] = blocks[blocks.size()-1] + task_name[i];
	}

	vector<int> replacement;
	int currentArgPos = 0;
	for (size_t task = 1; task < blocks.size(); task += 2){
		instantiated_plan_step ps;
		ps.declaredPrimitive = true;
		ps.name = blocks[task];
		for (int arg = 0; arg < stoi(blocks[task+1]); arg++)
			ps.arguments.push_back(expanded_ps.arguments[currentArgPos + arg]);

		currentArgPos += stoi(blocks[task+1]);

		int id = get_next_free_id(plan);
		plan.tasks[id] = ps;
		replacement.push_back(id);
	}

	plan.tasks.erase(expanded_task);

	// find the place where it occurs
	for (auto & method : plan.subtasksForTask){
		if (count(method.second.begin(),method.second.end(),expanded_task)){
			vector<int> newSubtasks;
			for (int i : method.second)
				if (i != expanded_task)
					newSubtasks.push_back(i);
				else
					for(int j : replacement)
						newSubtasks.push_back(j);
			method.second = newSubtasks;
		}
	}

	// modify primitive plan
	vector<int> newPlan;
	plan.pos_in_primitive_plan.clear();
	for (int i : plan.primitive_plan)
		if (i != expanded_task){
			plan.pos_in_primitive_plan[i] = newPlan.size();
			newPlan.push_back(i);
		} else {
			for(int j : replacement){
				plan.pos_in_primitive_plan[j] = newPlan.size();
				newPlan.push_back(j);
			}
		}

	plan.primitive_plan = newPlan;

	return plan;
}

parsed_plan expand_compressed_method(parsed_plan & plan, int expanded_task){
	// get elements of this method
	string method_name = plan.appliedMethod[expanded_task];
	vector<int> method_subtasks = plan.subtasksForTask[expanded_task];

	int level = 0;
	vector<string> blocks; blocks.push_back("");
	for (size_t i = 1; i < method_name.size()-1; i++){
		if (level == 0 && method_name[i] == ';'){
			blocks.push_back("");
			continue;
		}
		
		if (method_name[i] == '<') level++;
		if (method_name[i] == '>') level--;
		blocks[blocks.size()-1] = blocks[blocks.size()-1] + method_name[i];
	}

	string main_method = blocks[0];
	auto [decomposed_task, decomposed_task_arguments] = parse_task_with_arguments_from_braced_expression(blocks[1]);
	string applied_method = blocks[2];
	int decomposed_id = stoi(blocks[3]);
	replace(blocks[4].begin(), blocks[4].end(), ',', ' ');
	vector<int> subtask_translation = parse_list_of_integers (blocks[4],0);
	assert(subtask_translation.size() == method_subtasks.size());	

	/*cout << main_method << endl;
	cout << decomposed_task << endl;
	cout << applied_method << endl;
	cout << decomposed_id << endl;*/


	// extracting the inner method
	map<int,int> inner_method_position_to_ID; // global ID
	for (size_t i = 0; i < subtask_translation.size(); i++){
		if (subtask_translation[i] >= 0) continue; // this was a task in the original method
		int task_position = -subtask_translation[i]-1;
		inner_method_position_to_ID[task_position] = method_subtasks[i];
	}

	// add a new task to the list of tasks, the one in the middle
	int new_ps_id = get_next_free_id(plan);
	instantiated_plan_step new_ps;
	new_ps.name = decomposed_task;
	new_ps.arguments = decomposed_task_arguments;
	new_ps.declaredPrimitive = false;
	plan.tasks[new_ps_id] = new_ps;
	plan.appliedMethod[new_ps_id] = applied_method;
	for (auto subtask : inner_method_position_to_ID)
		plan.subtasksForTask[new_ps_id].push_back(subtask.second);


	// change the main method
	plan.appliedMethod[expanded_task] = main_method;
	map<int,int> main_method_position_to_ID; // global ID
	for (size_t i = 0; i < subtask_translation.size(); i++){
		if (subtask_translation[i] < 0) continue; // this was a task in the expanded method
		main_method_position_to_ID[subtask_translation[i]] = method_subtasks[i];
	}
	main_method_position_to_ID[decomposed_id] = new_ps_id;
	plan.subtasksForTask[expanded_task].clear();
	for (auto subtask : main_method_position_to_ID)
		plan.subtasksForTask[expanded_task].push_back(subtask.second);

	return plan;
}


void compress_artificial_primitives(parsed_plan & plan, set<int> prims){
	// remove the primitive task from the plan
	int decrement = 0;
	vector<int> newPrimPlan;
	for(size_t i = 0; i < plan.primitive_plan.size(); i++){
		if (prims.count(plan.primitive_plan[i])){
			plan.pos_in_primitive_plan.erase(plan.primitive_plan[i]);
			decrement++;
		} else {
			// removed a tasks before this one
			plan.pos_in_primitive_plan[plan.primitive_plan[i]] -= decrement;
			if (plan.pos_in_primitive_plan[plan.primitive_plan[i]]!= newPrimPlan.size()){
				cout << color(COLOR_RED,"Bug! ...") << endl;
				exit(1);
			}
			newPrimPlan.push_back(plan.primitive_plan[i]);
		}
	}


	if (decrement != prims.size()){
		cout << color(COLOR_RED,"Not all primitives were in the primitive plan ..." + to_string(decrement) + " < " + to_string(prims.size())) << endl;
		exit(1);
	}
	// erase the tasks from the plan
	plan.primitive_plan = newPrimPlan; 


	for (int expanded_task : prims){
		// find the rule where expanded is contained
		int contained_in_task = -1; // -1 will mean it is contained in root
		if (plan.task_contained_in.count(expanded_task))
			contained_in_task = plan.task_contained_in[expanded_task];



		vector<int> current_ids;
		if (contained_in_task == -1) current_ids = plan.root_tasks;
		else current_ids = plan.subtasksForTask[contained_in_task];

		// put the ids of the task we have to expand back to the place where it stems from
		vector<int> new_ids;
		for (int id : current_ids){
			if (id == expanded_task){
			} else
				new_ids.push_back(id);
		}

		if (contained_in_task == -1) plan.root_tasks = new_ids;
		else plan.subtasksForTask[contained_in_task] = new_ids;

		plan.tasks.erase(expanded_task);
		plan.appliedMethod.erase(expanded_task);
		plan.subtasksForTask.erase(expanded_task);

	}

}


parsed_plan compress_artificial_method(parsed_plan & plan, int expanded_task){
	vector<int> subtasks_of_expanded;
   	if (plan.tasks[expanded_task].declaredPrimitive){
		// remove the primitive task from the plan
		int pos_of_prim = -1;
		for(size_t i = 0; i < plan.primitive_plan.size(); i++){
			if (plan.primitive_plan[i] == expanded_task){
				pos_of_prim = i;
			} else if (pos_of_prim != -1){
				// removed a task before this one
				plan.pos_in_primitive_plan[plan.primitive_plan[i]]--;
			}
		}
		if (pos_of_prim == -1){
			cout << color(COLOR_RED,"Declared primitive " + to_string(expanded_task)
				   	+ " not contained in primitive plan.") << endl;
			exit(1);
		}

		// erase the task from the plan
		plan.primitive_plan.erase(plan.primitive_plan.begin() + pos_of_prim);
	} else {
		subtasks_of_expanded = plan.subtasksForTask[expanded_task];
	}

	// find the rule where expanded is contained
	int contained_in_task = -1; // -1 will mean it is contained in root
	if (plan.task_contained_in.count(expanded_task))
		contained_in_task = plan.task_contained_in[expanded_task];

	//for (auto sub : plan.subtasksForTask){
	//	for (int subtask : sub.second)
	//		if (expanded_task == subtask){
	//			contained_in_task = sub.first;
	//			break;
	//		}
	//	if (contained_in_task != -1) break;
	//}

	vector<int> current_ids;
	if (contained_in_task == -1) current_ids = plan.root_tasks;
	else current_ids = plan.subtasksForTask[contained_in_task];

	// put the ids of the task we have to expand back to the place where it stems from
	vector<int> new_ids;
	for (int id : current_ids){
		if (id == expanded_task){
			for (int st : subtasks_of_expanded)
				new_ids.push_back(st);
		} else
			new_ids.push_back(id);
	}

	if (contained_in_task == -1) plan.root_tasks = new_ids;
	else plan.subtasksForTask[contained_in_task] = new_ids;
	for (int subtask : new_ids)
		plan.task_contained_in[subtask] = contained_in_task;

	plan.tasks.erase(expanded_task);
	plan.appliedMethod.erase(expanded_task);
	plan.subtasksForTask.erase(expanded_task);

	return plan;
}

void convert_plan(parsed_plan & plan){
	//cout << endl << endl << "PLAN =========================================================" << endl;
	//print_plan(plan, cout);	
	// look for things that are not ok ..

	// start with expansion compiled things (i.e. where we introduced *new* tasks and methods)
	vector<int> methods_to_compress;
	for (auto method : plan.appliedMethod){
		if (method.second[0] == '_' && method.second[1] == '!'){
			methods_to_compress.push_back(method.first);
		}
	}
	if (methods_to_compress.size()) {
		update_index(plan);
		for (int m : methods_to_compress)
			compress_artificial_method(plan,m);
		convert_plan(plan);
		return;
	}

	vector<int> expand;

	for (auto task : plan.tasks){
		if (task.second.name[0] == '%'){
			expand.push_back(task.first);
		}
		
		if (task.second.name[0] == '_' && task.second.name[1] == '!'){
			methods_to_compress.push_back(task.first);
		}
	}
	if (methods_to_compress.size() + expand.size()){
		update_index(plan);
		for (int m : methods_to_compress)
			compress_artificial_method(plan,m);
		for (int m : expand)
			expand_compressed_action(plan,m);

		convert_plan(plan);
		return;
	}

	// first expand all compressed methods
	for (auto method : plan.appliedMethod){
		if (method.second[0] == '<'){
			expand.push_back(method.first);
		}
	}
	
	if (expand.size()){ 
		for (int m : expand)
			expand_compressed_method(plan,m);
		convert_plan(plan);
		return;
	}

	//cout << "Compiled entries" << endl;	
	// only then remove compiled entries. This removal my make expansion rules in methods names impossible
	for (auto method : plan.appliedMethod){
		if (method.second[0] == '_'){
			methods_to_compress.push_back(method.first);
		}
	}
	
	if (methods_to_compress.size()) {
		update_index(plan); // index needs to be updated
		for (int m : methods_to_compress){
			compress_artificial_method(plan,m);
		}
		convert_plan(plan);
		return;
	}
	//update_index(plan);
	
	set<int> compress_primitives;
	for (auto task : plan.tasks){
		if (task.second.name[0] == '_'){
			if (plan.tasks[task.first].declaredPrimitive)
				compress_primitives.insert(task.first);
			else
				methods_to_compress.push_back(task.first);
			//compress_artificial_method(plan,task.first);
		}
	}
	if (methods_to_compress.size() + compress_primitives.size()){
		compress_artificial_primitives(plan, compress_primitives);
		update_index(plan);
		for (int m : methods_to_compress)
			compress_artificial_method(plan,m);
		convert_plan(plan);
		return;
	}


	// sanitise task names
	for (auto & task : plan.tasks){
		auto it = task.second.name.find('|');
		if (it == string::npos) continue;
		// remove everything after the |  -- this is stuff that was added by compilers
		task.second.name.erase(task.second.name.begin() + it,task.second.name.end());
	}
}



void convert_plan(istream & plan, ostream & pout){
	parsed_plan initial_plan = parse_plan(plan, 0);

	// in-place conversion	
	convert_plan(initial_plan);
	parsed_plan converted_plan = initial_plan;

	print_plan(converted_plan, pout);	
}



