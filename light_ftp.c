/* 
** light_ftp.c download or upload file to ftp server
** author: onlyu
** date: 2011-4-24
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
 
#include <curl/curl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#define FAILED -1
#define SUCCESS 0
 
#define METHOD_PUT 1
#define METHOD_GET 2
/* NOTE: if you want this example to work on Windows with libcurl as a
   DLL, you MUST also provide a read callback with CURLOPT_READFUNCTION.
   Failing to do so will give you a crash since a DLL may not use the
   variable's memory when passed in to it from an app like this. */ 
static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
  /* in real-world cases, this would probably get this data differently
     as this fread() stuff is exactly what the library already would do
     by default internally */ 
  printf("+");
  size_t retcode = fread(ptr, size, nmemb, stream);
  return retcode;
}

static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
  printf("+");
  size_t retcode = fwrite(ptr, size, nmemb, stream);
  return retcode;
}
 
void display_usage(void)
{
  printf("usage:light_ftp [-G|-P] [-u username -p passwd] -l localfile [-r remotefie] -U url\n");
  printf("or:light_ftp [--put|--get] [--user username --password passwd] --local-file localfile [--remote-file remotefile] --url url\n");
  exit(FAILED);
  return;
}

int do_download(char *url, char *usr, char *pwd, char *local_file, char *remote_file)
{
  CURL *curl;
  CURLcode res;
  FILE *fd_dest;
  int ret;

  static char usr_pwd[255] = {0};
  static char remote_url[1024] = {0};
  static char local_tmp_file[1024] = {0};
  
  sprintf(usr_pwd, "%s:%s", usr, pwd);
  sprintf(remote_url, "%s/%s", url, remote_file);
  sprintf(local_tmp_file, "%s.tmp", local_file);

  /* make sure the file's intermediate directory exist */
  static char cmd[1024] = {0};
  sprintf(cmd,"dirname %s | xargs mkdir -p ", local_tmp_file);
  system(cmd);

  /* open the file for write */
  fd_dest = fopen(local_tmp_file, "wb");

  if(!fd_dest){
    return FAILED;
  }

  printf("getting file %s:", remote_url);
  curl_global_init(CURL_GLOBAL_ALL);
  
  /* get a curl handle */ 
  curl = curl_easy_init();
  if(curl) {
    if(strlen(usr_pwd)>0){
      curl_easy_setopt(curl, CURLOPT_USERPWD, usr_pwd);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fd_dest);

    curl_easy_setopt(curl, CURLOPT_URL, remote_url);
    /* Switch on full protocol/debug output */ 
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
 
    res = curl_easy_perform(curl);
 
    /* always cleanup */ 
    curl_easy_cleanup(curl);
 
    if(CURLE_OK != res) {
      /* we failed */ 
      fprintf(stderr, "curl told us %d\n", res);
      ret = FAILED;
    }else{
      ret = SUCCESS;
    }
  }else{
    ret = FAILED;
  }

  fclose(fd_dest);
  curl_global_cleanup();

  // rename the file
  static char rename[1024] = {0};
  sprintf(rename, "mv %s %s", local_tmp_file, local_file);
  system(rename);
  return ret;
}

int do_upload(char *url, char *usr, char *pwd, char *local_file, char *remote_file)
{
  CURL *curl;
  CURLcode res;
  FILE *hd_src;
  struct stat file_info;
  curl_off_t fsize;
 
  struct curl_slist *headerlist=NULL;
  static char buf_1[1024] = {0};
  static char buf_2[1024] = {0};
  static char usr_pwd[256] = {0};
  static char remote_url[1024] = {0};

  strcat(buf_1, "RNFR ");
  strcat(buf_1, remote_file);
  //strcat(buf_1, ".tmp");

  strcat(buf_2, "RNTO ");
  strcat(buf_2, remote_file);

  strcat(usr_pwd, usr);
  strcat(usr_pwd, ":");
  strcat(usr_pwd, pwd);

  strcat(remote_url, url);
  strcat(remote_url, "/");
  strcat(remote_url, remote_file);
  //strcat(remote_url, ".tmp");

  /* get the file size of the local file */ 
  if(stat(local_file, &file_info)) {
    printf("Couldnt open '%s': %s\n", local_file, strerror(errno));
    return 1;
  }
  fsize = (curl_off_t)file_info.st_size;
 
  //printf("Local file size: %" CURL_FORMAT_CURL_OFF_T " bytes.\n", fsize);
  printf("putting file %s/%s [%"CURL_FORMAT_CURL_OFF_T"]:", url, remote_file, fsize);
 
  /* get a FILE * of the same file */ 
  hd_src = fopen(local_file, "rb");
 
  /* In windows, this will init the winsock stuff */ 
  curl_global_init(CURL_GLOBAL_ALL);
 
  /* get a curl handle */ 
  curl = curl_easy_init();
  if(curl) {
    /* build a list of commands to pass to libcurl */ 
    headerlist = curl_slist_append(headerlist, buf_1);
    headerlist = curl_slist_append(headerlist, buf_2);
 
    if(strlen(usr_pwd) > 0){
      curl_easy_setopt(curl, CURLOPT_USERPWD, usr_pwd);
    }

    
    /* we want to use our own read function */ 
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    
    /* enable uploading */ 
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
 
    /* specify target */ 
    curl_easy_setopt(curl,CURLOPT_URL, remote_url);
 
    /* pass in that last of FTP commands to run after the transfer */ 
    curl_easy_setopt(curl, CURLOPT_POSTQUOTE, headerlist);
 
    /* now specify which file to upload */ 
    curl_easy_setopt(curl, CURLOPT_READDATA, hd_src);
 
    /* Set the size of the file to upload (optional).  If you give a *_LARGE
       option you MUST make sure that the type of the passed-in argument is a
       curl_off_t. If you use CURLOPT_INFILESIZE (without _LARGE) you MUST       make sure that to pass in a type 'long' argument. */ 
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                     (curl_off_t)fsize);
 
    /* Now run off and do what you've been told! */ 
    res = curl_easy_perform(curl);
 
    /* clean up the FTP commands list */ 
    curl_slist_free_all (headerlist);
 
    /* always cleanup */ 
    curl_easy_cleanup(curl);
  }
  fclose(hd_src); /* close the local file */ 
 
  curl_global_cleanup();
  return SUCCESS;
}


struct global_arg_t{
  char *usr;
  char *pwd;
  char *url;
  char *local_file;
  char *remote_file;
  int method; // get or put
}global_arg;

int main(int argc, char **argv)
{

  static const char *global_opt_string = "GPu:p:l:r:C:h?";

  static const struct option global_long_opts[] = {
    { "user", required_argument, NULL, 'u' }, // user name
    { "password", required_argument, NULL, 'p' }, // password
    { "local-file", required_argument, NULL, 'l' }, // local file name
    { "remote-file", required_argument, NULL, 'r' }, // remote file name
    { "url", required_argument, NULL, 'U'}, // remote url
    { "get", no_argument, NULL, 'G' }, // use get method
    { "put", no_argument, NULL, 'P' }, // use put method
    { "help", no_argument, NULL, 'h'}, // help
    { NULL, no_argument, NULL, 0 },
  };

  /* parse the arguments */
  int opt = 0;
  int long_index = 0;
  global_arg.usr = NULL;
  global_arg.pwd = NULL;
  global_arg.url = NULL;
  global_arg.local_file = NULL;
  global_arg.remote_file = NULL;
  global_arg.method = METHOD_GET;

  opt = getopt_long(argc, argv, global_opt_string, global_long_opts, &long_index);

  while(opt != -1){
    switch(opt){
    case 'u':
      global_arg.usr = optarg;
      break;
    case 'p':
      global_arg.pwd = optarg;
      break;
    case 'l':
      global_arg.local_file = optarg;
      break;
    case 'r':
      global_arg.remote_file = optarg;
      break;
    case 'U':
      global_arg.url = optarg;
      break;
    case 'G':
      global_arg.method = METHOD_GET;
      break;
    case 'P':
      global_arg.method = METHOD_PUT;
      break;
    case 'h':
    case '?':
      display_usage();
      break;
    default:
      printf("wrong option:%c\n",opt);
      break;
    }
    opt = getopt_long(argc, argv, global_opt_string, global_long_opts, &long_index);
  }

#ifdef _DEBUG
  printf("arguments:\n");
  printf("user name:%s\n", global_arg.usr);
  printf("password:%s\n", global_arg.pwd);
  printf("locale file:%s\n", global_arg.local_file);
  printf("remote file:%s\n", global_arg.remote_file);
  printf("remote url:%s\n", global_arg.url);
  printf("method:%s\n", global_arg.method == METHOD_GET?"get":"put");
#endif

  // check the arguments
  if(!global_arg.url || strlen(global_arg.url) == 0){
    printf("error:not remote url or bad url!!!\n");
    display_usage();
  }

  if(global_arg.usr && !global_arg.pwd){
    printf("error:not password provide!!!\n");
    display_usage();
  }

  if(!global_arg.usr && global_arg.pwd){
    printf("warning:no user, ignore password!!!\n");
  }
  
  if(global_arg.method == METHOD_PUT){
    if(!global_arg.local_file || strlen(global_arg.local_file) == 0){
      printf("error:put method must have local file name!!!\n");
      display_usage();
    }
    if(!global_arg.remote_file || strlen(global_arg.remote_file) == 0){
      global_arg.remote_file = global_arg.local_file;
      //printf("warning:no argument remote file!!!\n");
    }
  }else{
    if(!global_arg.remote_file || strlen(global_arg.remote_file) == 0){
      printf("error:get method must have remote file name!!!\n");
      display_usage();
    }
    if(!global_arg.local_file || strlen(global_arg.local_file) == 0){
      global_arg.local_file = global_arg.remote_file;
      //printf("warning:no argument local file!!!\n");
    }
  }

  if(global_arg.method == METHOD_GET){
    do_download(global_arg.url, 
		global_arg.usr,
		global_arg.pwd,
		global_arg.local_file,
		global_arg.remote_file);
  }else{
    // call the upload function
    do_upload(global_arg.url, 
	      global_arg.usr, 
	      global_arg.pwd, 
	      global_arg.local_file, 
	      global_arg.remote_file);
  }
	
  printf("\n");
  return SUCCESS;
}
