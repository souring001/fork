#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 50
#define MAX_PIPE 50
#define MAX_LEN  500
#define MAX_HIS 500

char *argv[MAX_PIPE + 1][MAX_ARGS];
int fd[MAX_PIPE][2], status, bgflag;
pid_t pid[MAX_PIPE + 1], pidb;

void p_fork(int pflag, int cnt){
	if(pflag - cnt > 0) pipe(fd[cnt]);
	pid[cnt] = fork();
	if(pid[cnt] == -1){
		perror("fork");
		exit(-1);
	}else if(pid[cnt] == 0){
		if(bgflag && cnt == 0){
			//バックグラウンド実行のための孫プロセエス生成
			pidb = fork();
			if(pidb == -1){
				perror("fork");
				exit(-1);
			}else if(pidb == 0){
				//孫
				if(pflag == 0){
					//何もしない
				}else if(cnt == 0){
					//最初の子
					dup2(fd[cnt][1], 1);
					close(fd[cnt][1]);
					close(fd[cnt][0]);
				}
				fflush(stdout);
				execvp(argv[cnt][0], argv[cnt]);
				exit(0);	//不明な入力を受け付けたとき終わらずに分裂が起こってしまうことの防止
			}else{
				//子は直ちにexitする
				exit(0);
			}
		}else{
			//バックグラウンド実行でない or 多段パイプの処理 は孫不要
			if(pflag == 0){
				//何もしない
			}else if(cnt == 0){
				//最初の子
				dup2(fd[cnt][1], 1);
				close(fd[cnt][1]);
				close(fd[cnt][0]);
			}else if(pflag - cnt == 0){
				//最後の子
				dup2(fd[cnt-1][0], 0);
				close(fd[cnt-1][0]);
			}else if(pflag - cnt > 0){
				//途中の子
				dup2(fd[cnt][1], 1);
				dup2(fd[cnt-1][0], 0);
				close(fd[cnt][1]);
				close(fd[cnt][0]);
				close(fd[cnt-1][0]);
			}
			fflush(stdout);
			execvp(argv[cnt][0], argv[cnt]);
			exit(0);	//不明な入力を受け付けたとき終わらずに分裂が起こってしまうことの防止
		}
	}else{
		//親
		if(pflag - cnt == 0 && pflag) close(fd[cnt-1][0]);
		if(pflag - cnt > 0){
			close(fd[cnt][1]);
			p_fork(pflag, cnt+1);
		}
		wait(&status);
	}
}

int main(void){
    int argc, n = 0, pflag, i, d;
    char input[MAX_LEN], history[MAX_HIS][MAX_LEN], *argpipe[MAX_ARGS], *cp;
    const char *delim = " \t\n", *delim2 = "|"; /* コマンドのデリミタ(区切り文字) */

    while (1) {
    	bgflag = 0;
        ++n;
        
        /* プロンプトの表示 */
        printf("command[%d] ", n);

        /* 1行読み込む */
        if (fgets(input, sizeof(input), stdin) == NULL) {
            /* EOF(Ctrl-D)が入力された */
            printf("Goodbye!\n");
            exit(0);
        }
        
        /* ヒストリーの保存 */
        strcpy(history[n-1], input);
        
        /* 最後にバックグラウンド実行の&があるかのチェック */
        if(input[strlen(input)-2] == '&'){
        	bgflag = 1;
        	input[strlen(input)-2] = '\0';
        }
        
        /* まずパイプで分割する. pflagはパイプの個数 */
        cp = input;
        for (argc = 0; argc < MAX_ARGS; argc++) {
            if((argpipe[argc] = strtok(cp,delim2)) == NULL){
            	pflag = argc-1;		//パイプの数
            	if(pflag > MAX_PIPE){
            		printf("Too many!\n");
            		exit(0);
            	}
               break;
            }
            cp = NULL;
        }

        /* コマンド行を空白/タブで分割し，配列 argv[] に格納する */
        for(i = 0; i <= pflag; i++){
        	cp = argpipe[i];
        	for (argc = 0; argc < MAX_ARGS; argc++) {
           	 if ((argv[i][argc] = strtok(cp,delim)) == NULL){
           	     if(i == 0 && argc ==0)goto ctn;
           	     break;
           	 }
            	cp = NULL;
        	}
        }
        goto ok;
        ctn:
        	n--;
        	continue;	//何も入力せずにEnterした場合の対策
        ok:
              
        /* exitが入力されたら終了する */
        if(!strcmp(argv[0][0], "exit") && pflag == 0){
       	    printf("Have a nice day!\n");
            exit(0);
        }else if(!strcmp(argv[0][0], "cd") && pflag == 0){
       	    chdir(argv[0][1]);
            continue;
        }else if(!strcmp(argv[0][0], "!!") && pflag == 0){
        /* !!が入力されたら1つ前の実行 */ 
        	if(n==1){
        		n--;
        		continue;
        	}
  	     	strcpy(input, history[n-2]);
        	strcpy(history[n-1], input);
        	
        	if(input[strlen(input)-2] == '&'){
        		bgflag = 1;
        		input[strlen(input)-2] = '\0';
        	}
        	
        	cp = input;
       		for (argc = 0; argc < MAX_ARGS; argc++) {
     		       if((argpipe[argc] = strtok(cp,delim2)) == NULL){
      	 		 	pflag = argc-1;	
		               break;
		 	}
		cp = NULL;
        	}
	        for(i = 0; i <= pflag; i++){
        		cp = argpipe[i];
        		for (argc = 0; argc < MAX_ARGS; argc++) {
           		if ((argv[i][argc] = strtok(cp,delim)) == NULL)
           	     		break;
            		cp = NULL;
        		}
       		}
        }else if(*argv[0][0] == '!' && pflag == 0){
        /* !nが入力されたらhistoryのn番目の実行 */
        	argv[0][0]++;
        	d = atoi(argv[0][0]);
        	if(d <= 0 || n <= d){	//dが範囲外は何もしない
        		n--;
        		continue;
        	}
  	     	strcpy(input, history[d-1]);
        	strcpy(history[n-1], input);
        	
        	if(input[strlen(input)-2] == '&'){
        		bgflag = 1;
        		input[strlen(input)-2] = '\0';
        	}
        	
        	cp = input;
       		for (argc = 0; argc < MAX_ARGS; argc++) {
     		       if((argpipe[argc] = strtok(cp,delim2)) == NULL){
      	 		 	pflag = argc-1;	
		               break;
		 	}
		cp = NULL;
        	}
	        for(i = 0; i <= pflag; i++){
        		cp = argpipe[i];
        		for (argc = 0; argc < MAX_ARGS; argc++) {
           		if ((argv[i][argc] = strtok(cp,delim)) == NULL)
           	     		break;
            		cp = NULL;
        		}
       		}
        }
        
           
        if(!strcmp(argv[0][0], "history") && pflag == 0){
        /* historyが入力されたら履歴を表示 historyとパイプは同時使用できない */ 
        	for(i = 0; i < n; i++){
        		printf("%5.d  %s", i+1, history[i]);
        	}
        	continue;
        }
 
	p_fork(pflag, 0);


        /*
          入力したコマンドを実行する
	exit, ctl+Dで終了
	多段パイプ可能
	cdコマンドも実行可能
	ヒストリ機能(history, !!, !5)もある
          */
    }
}
