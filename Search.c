#ifdef PATH_MAX
static long pathmax = PATH_MAX;
#else
static long pathmax = 0;
#endif

#ifndef MAX_BUF
#define MAX_BUF 200
#endif

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include<stdlib.h>
#include<stdbool.h>
#include <sys/wait.h>

typedef int MyFunc(const char*, const struct stat*, int, int);

static long posix_version = 0;
static long xsi_version = 0;

#define PATH_MAX_GUESS 1024

char path[MAX_BUF];
static MyFunc myfunc;
static int myftw(char*, MyFunc *);
static int dopath(MyFunc *, int);

static long nreg, ndir, nblk, nchr, nfifo, nslink, nsock, ntot;

//for recording requirements
bool S_opt = 0;
bool s_opt = 0;
bool f_opt = 0;
bool t_opt = 0;
bool E_opt = 0;
bool e_opt = 0;

static long int minimum_length = 100;
char substrp[MAX_BUF];
char cmd[MAX_BUF]; //stores command
char ** vectors = NULL; //stores parsed command arguments
char crit_for_filter; //criteria for filtering
char* filtered_files; //filtered files

#define FTW_F 1
#define FTW_D 2
#define FTW_DNR 3
#define FTW_NS 4

static char *absolute_location;
static size_t pathlen;

void ec_E();
char * path_alloc(size_t *sizep){

	char *ptr;
	size_t size;
	
	if (posix_version == 0)
		posix_version = sysconf(_SC_VERSION);
	if(xsi_version == 0)
		xsi_version = sysconf(_SC_XOPEN_VERSION);
		
	if(pathmax == 0)
	{
	
		errno = 0;
		
		if((pathmax = pathconf("/", _PC_PATH_MAX)) < 0){
		
			if(errno == 0)
				pathmax = PATH_MAX_GUESS;
			else
				printf("pathconf error for _PC_PATH_MAX");
		}
		else{

			pathmax++;
		
		}
		
	}
	
	if((posix_version < 200112L) && (xsi_version < 4))
		size = pathmax+1;
	else
		size = pathmax;
		
	
	if((ptr = malloc(size)) == NULL)
		printf("malloc error for pathname");
	
	if(sizep!=NULL)
		*sizep = size;
		
		
	return(ptr);
		
}	
	

static int myftw(char *pathname, MyFunc *func){

	absolute_location = path_alloc(&pathlen);

	if(pathlen <= strlen(pathname)){
		
		pathlen = strlen(pathname) * 2;
	
		if((absolute_location = realloc(absolute_location, pathlen)) == NULL){
			printf("reallocation failed!");		
			exit(0);
		}
	
	}
	
	strcpy(absolute_location, pathname);
	
	return(dopath(func, 0));
	
}

static int dopath(MyFunc * func, int depth){
	
	struct stat statbuf;
	struct dirent *dirp;
	DIR *dp;
	int result, n;
	
	if(lstat(absolute_location, &statbuf) < 0){

		return(func(absolute_location, &statbuf, FTW_NS, depth));
		
	}
	if(S_ISDIR(statbuf.st_mode) == 0){

		return(func(absolute_location, &statbuf, FTW_F, depth));
		
	}
	
	if((result = func(absolute_location, &statbuf, FTW_D, depth)) != 0){

		return result;
		
	}
	
	n = strlen(absolute_location);
	
	if(n + NAME_MAX + 2 > pathlen){
		
		pathlen *= 2;
		
		if((absolute_location = realloc(absolute_location, pathlen)) == NULL)
			printf("realloc failed");
			exit(0);
			
	}

	absolute_location[n] = '/';
	n++;
	
	absolute_location[n] = 0;
	
	if ((dp = opendir(absolute_location)) == NULL){
		
		return(func(absolute_location, &statbuf, FTW_DNR, depth));
		
	}
	
	while ((dirp = readdir(dp)) != NULL){
		
		if(strcmp(dirp->d_name, ".") == 0 || strcmp(dirp->d_name, "..") == 0)
			continue;
			
		strcpy(&absolute_location[n], dirp->d_name);
		
		if((result = dopath(func, depth+1)) != 0)
			break;
		
	}
	
	absolute_location[n-1] = 0;
	
	if(closedir(dp) < 0){
		
		printf("unable to close the directory at %s", absolute_location);
		return(0);
		
	}
	return result;	
}

void insert_in_arr(const char* absolute_location){

	strcat(filtered_files, absolute_location);
	strcat(filtered_files, " ");		
	
}
	
void get_subdir(const char *absolute_location, int depth){

	struct stat statInst;
	stat(absolute_location, &statInst);

	if(s_opt && statInst.st_size < minimum_length){
	
		return;
	
	}

	if(f_opt && strstr(absolute_location, substrp) == NULL){
	
		return;
		
	}
	
	if(E_opt){
	
		insert_in_arr(absolute_location);
	
	}

	for(int i = 0; i < depth; i++)
		printf("\t");

	int fs_count = 0; //forward slash count

	for(int i = 0; absolute_location[i] != '\0'; i++){
	
		if(absolute_location[i] == '/')
			fs_count = fs_count + 1;
	
	}

	for(int i = 0; absolute_location[i] != '\0'; i++){
	
		if(fs_count == 0)
			printf("%c", absolute_location[i]);
	
		if(absolute_location[i] == '/')
			fs_count = fs_count - 1;
	
	}
	
	if(S_opt == true)
		printf(" %ld", statInst.st_size);
	
	printf("\n");

}	

int parse_cnt = 1;

void parse(const char* filepath){

	if(vectors == NULL){
		
		for(int i = 0; cmd[i] != '\0'; i++){
		
			if(cmd[i] == ' ')
				parse_cnt++;	
		
		}

		int * lengths = malloc(sizeof(int) * parse_cnt);
		
		for(int i = 0; i < parse_cnt; i++){
		
			lengths[i] = 0;
		
		}
		
		int index = 0;
		for(int i = 0; cmd[i] != '\0'; i++){
		
			if(cmd[i] == ' ')
				index++;
			else
				lengths[index] ++;
		
		}

		if(filepath)
			vectors = malloc(sizeof(char*) * (parse_cnt + 2));
		else	
			vectors = malloc(sizeof(char*) * (parse_cnt+1));

		for(int i = 0; i < parse_cnt; i++)
			vectors[i] = malloc(lengths[i] * sizeof(char));

		for(int i = 0; i < parse_cnt; i++){
		
			for(int j = 0; j < lengths[i]; j++)
				vectors[i][j] = '\0';
		}
			
		int j = 0, k = 0;
		
		for(int i = 0; 1; i++){
		
			if(cmd[i] == ' '){

				vectors[j][k] = '\0';
							
				k = 0;
				j++;
				continue;	
				
			}
		
			vectors[j][k] = cmd[i];
			k++;
			
			if(cmd[i] == '\0')
				break;
		
		}

	
	vectors[parse_cnt] = malloc(1000*sizeof(char));
	
	for(int x = 0; x < 1000; x++)
		vectors[parse_cnt][x] = '\0';
	
	}
	
	if(filepath)
		strcpy(vectors[parse_cnt], filepath);
	else
		vectors[parse_cnt] = NULL;
		
	vectors[parse_cnt + 1] = NULL;



}

char** files_vector;

char** combine_vectors(){

	char** merged;
	
	int size = 0;
	
	for(int i = 0; 1; i++){
	
		if(vectors[i] != NULL)
			size++;
		else
			break;
	
	}

	for(int i = 0; 1; i++){
	
		if(files_vector[i] != NULL && strcmp(files_vector[i], " ") != 0)
			size++;
		else
			break;
	
	}
	
	merged = malloc(sizeof(char*) * (size+1));
	
	int index = 0;
	
	for(int i = 0; 1; i++){
	
		if(vectors[i] != NULL){
		
			merged[index] = vectors[i];
			index++;
			
		}
		else
			break;
	
	}
	
	for(int i = 0; 1; i++){
	
		if(files_vector[i] != NULL){
		
			merged[index] = files_vector[i];
			index++;
		
		}
		else
			break;
	
	}	
	
	printf("merged...");
	merged[size] = NULL;
	
	return merged;

}

void vectorize_matching_files(){

	int filesCount = 0;

	for(int i = 0; filtered_files[i] != '\0'; i++){

		if(filtered_files[i] == ' ')
			filesCount++;	

	}

	int * lengths = malloc(sizeof(int) * filesCount);

	for(int i = 0; i < filesCount; i++){

		lengths[i] = 0;

	}

	int index = 0;
	
	for(int i = 0; filtered_files[i] != '\0'; i++){

		if(filtered_files[i] == ' ')
			index++;
		else
			lengths[index] = lengths[index] + 1;

	}


	files_vector = malloc(sizeof(char*) * (filesCount + 1));

	for(int i = 0; i < filesCount; i++)
		files_vector[i] = malloc(lengths[i] * sizeof(char));


	int j = 0, k = 0;

	for(int i = 0; filtered_files[i] != '\0'; i++){

		if(filtered_files[i] == ' '){
		
			k = 0;
			j++;
			continue;	
			
		}

		files_vector[j][k] = filtered_files[i];
		k++;

	}


	files_vector[filesCount] = NULL;


}

void ec_E()
{

	char *nofile = NULL;
	
	parse(nofile); 
	vectorize_matching_files();	
	
	char** execute = combine_vectors();
	
	sleep(1);
	
	pid_t pid = fork();	
	
	if(pid == 0){
		
		execvp("ls", vectors);
		exit(0);
		
	}
	else if(pid>0){
	
		wait(NULL);
	
	}
	
}

void ec_e(const char* filepath){

	struct stat statInst;
	stat(filepath, &statInst);

	if(s_opt && statInst.st_size < minimum_length){
	
		return;
	
	}

	if(f_opt && strstr(filepath, substrp) == NULL){
	
		return;
		
	}


	parse(filepath);
	
	pid_t pid = fork();
	
	if(pid == 0){
	
		execvp(vectors[0],vectors);
		exit(0);
		
	}
	else if(pid>0){
	
		wait(NULL);
	
	}

}
	
static int myfunc(const char *pathname, const struct stat *statptr, int type, int depth){
	
	switch (type){
		
		case FTW_F:
			
			if(!t_opt || crit_for_filter == 'f')		
				get_subdir(pathname, depth);	
				
			if(e_opt)
				ec_e(pathname);	
								
			switch (statptr -> st_mode & S_IFMT){
				
				case S_IFREG: nreg++; break;
				case S_IFBLK: nblk++; break;
				case S_IFCHR: nchr++; break;
				case S_IFIFO: nfifo++; break;
				case S_IFLNK: nslink++; break;
				case S_IFSOCK: nsock++; break;
				case S_IFDIR: printf("for S_IFDIR for %s", pathname);
	
			}
			break;
			
		case FTW_D:
		
			if(!t_opt || crit_for_filter == 'd') 	
				get_subdir(pathname, depth);
		
			ndir++;
			break;
		case FTW_DNR:
			printf("Unable to read directory %s", pathname); 
		case FTW_NS:
			printf("Stat error for %s", pathname);			
		
		default:
			printf("Unknown type %d for pathname %s", type, pathname);
			
		}
	return(0);
}
	
	
int main(int parse_cnt, char* argv[]){
	
	char *a1 = argv[1];
	
	for(int i = 0; i < MAX_BUF; i++)
		cmd[i] = 0;
	
	char option_argument;
	int result;
	
	while((option_argument = getopt(parse_cnt, argv, "Ss:t:f:E:e:")) != -1){
		
		if (option_argument == 'S'){
		
			S_opt = true;
	
		}
	
		else if (option_argument == 's'){
		
			s_opt = true;
			minimum_length = atoi(optarg);
		
		}
		else if (option_argument == 'f'){
		
			f_opt = true;
			strcpy(substrp, optarg);
		}
		
		else if (option_argument == 't'){
		
			t_opt = true;
			crit_for_filter = optarg[0];
		}
			
		else if (option_argument == 'E'){
		
			E_opt = true;
			filtered_files = malloc(sizeof(char) * 100000);
			strcpy(cmd, optarg);
		}
		else if (option_argument == 'e'){
			e_opt = true;
			strcpy(cmd, optarg);
		}
		
		else{
			
			//do nothing
			perror("Invalid Argument\n");
		}
	
	}
	
	//copy current working directory
	getcwd(path, MAX_BUF);
	
	if (argv[1] && strstr(a1, "/")){
		
		result = myftw(a1, myfunc);
		
	}
	else{
	
		result = myftw(path, myfunc);

	}
	
	//check for E command at the end
	if(E_opt)
		ec_E();	


	printf("\n");
	
	exit(result);
	
}
