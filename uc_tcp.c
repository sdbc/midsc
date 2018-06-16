/**********************************************
 * @(#) TCP SERVER Tools                      *
 * to suport Fiberized.IO
 **********************************************/
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <scsrv.h>

#include "reg.h"

extern ucontext_t *get_uc(int TCB_no);
extern int do_event(int TCB_no,int socket,int flg);

#if __WORDSIZE == 64
#define MAX_STACK 0X200000
#else
#define MAX_STACK 0X100000
#endif

#ifndef MIN
#define MIN(a,b) ((a)<(b))?(a):(b)
#endif

extern unsigned long set_sp(void *new_stack);
extern char *restore_sp(void *to,void *from,unsigned long bp,unsigned long size);
//timeout for second 
int RecvNet(int socket,char *buf,int n,int timeout,int TCB_no)
{
int bcount=0,br,ret;
int i;
int fflag;
ucontext_t *tc=NULL,*uc=get_uc(TCB_no);
unsigned long begin_stack,save_stack_size=0;

	if(socket<0) return SYSERR;
	if(!buf && n<0) return 0;
	fflag=fcntl(socket,F_GETFL,0);
	if(uc) {
		tc=uc->uc_link;
		fcntl(socket,F_SETFL,fflag|O_NONBLOCK); //�첽����
	} else {
		struct timeval tmout;
		tmout.tv_sec=timeout;
        	tmout.tv_usec=0;
        	ret=setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,(char *)&tmout,sizeof(tmout));
		if(ret) {
                	ShowLog(5,"%s:setsockopt err=%d,%s",__FUNCTION__,
                                errno,strerror(errno));
        	}
	}

	*buf=0;
	br=0;

	while(bcount<n){
		if((br=read(socket,buf,n-bcount))>0){
			bcount+=br;
			buf+=br;
			continue;
		}
		if(errno==EAGAIN) return TIMEOUTERR;
		if(br<0){
		    if(errno!=ECONNRESET)
			ShowLog(1,"%s:br=%d,err=%d,%s",__FUNCTION__,br,errno,strerror(errno));
		    break;
		}
//ShowLog(5,"RecvNet:read br=0,errno=%d,%s",errno,strerror(errno));
		if(bcount < n && uc) { //�л�����
ShowLog(5,"%s:create fiber",__FUNCTION__);
		      if(!uc->uc_stack.ss_size) {
//���������ջ֡			
			swapcontext(uc,uc);
#if __WORDSIZE == 64
			begin_stack=tc->uc_mcontext.gregs[REG_RSP];
			save_stack_size=uc->uc_mcontext.gregs[REG_RSP]-begin_stack;
#else
			if(uc->uc_mcontext.gregs[REG_SS] == uc->uc_link->uc_mcontext.gregs[REG_SS]) {
		  		begin_stack=tc->uc_mcontext.gregs[REG_ESP];
		  		save_stack_size=uc->uc_mcontext.gregs[REG_ESP]-begin_stack;
			}
#endif
			if(save_stack_size > MAX_STACK) save_stack_size=0; 
			if(save_stack_size==0) {	//�����Ҫ�����ջ̫֡�󣬾Ͳ�Ҫ�첽��
				uc=NULL;
				fcntl(socket,F_SETFL,fflag);
				continue;
			} 
//����fiber
			uc->uc_stack.ss_size=save_stack_size+2048;
			uc->uc_stack.ss_sp=malloc(uc->uc_stack.ss_size);
			if(!uc->uc_stack.ss_sp) {
				uc->uc_stack.ss_size=0;
				uc=NULL;
				fcntl(socket,F_SETFL,fflag);
				continue;
			}
//�����߳�ջ֡
			memcpy(uc->uc_stack.ss_sp-save_stack_size,(void *)(begin_stack-save_stack_size),save_stack_size);
//��ʵ�ʵ�rsp,rbpҲ������  ����Ҫһ��asm
			set_sp(uc->uc_stack.ss_sp-save_stack_size);
		      }
		        swapcontext(uc,uc);//��do_event֮ǰ���趨��ջ
		        if(tc != uc->uc_link) //do_event�󱻱��߳�������
		    	    continue;
			i=do_event(TCB_no,socket,0);//EPOOLIN
			swapcontext(uc,uc->uc_link); // thread escape
		}
	}
	if(uc) {
		if(uc->uc_stack.ss_sp) {// ����fiber
ShowLog(5,"%s:drop fiber",__FUNCTION__);
//�ָ��߳�ջ֡ ��Ҫһ��ASM
//���µ�uc->uc_link.uc_mcontext.gregs[REG_RSP];
#if __WORDSIZE == 64
			memcpy(restore_sp((void *)uc->uc_link->uc_mcontext.gregs[REG_RSP],
					uc->uc_stack.ss_sp,uc->uc_link->uc_mcontext.gregs[REG_RBP],
					save_stack_size),
				uc->uc_stack.ss_sp-save_stack_size,save_stack_size);
#else
			memcpy(restore_sp((void *)uc->uc_link->uc_mcontext.gregs[REG_ESP],
					uc->uc_stack.ss_sp,uc->uc_link->uc_mcontext.gregs[REG_EBP],
					save_stack_size),
				uc->uc_stack.ss_sp-save_stack_size,save_stack_size);
#endif
//��Ҫ�ָ������Ĵ�����
			free(uc->uc_stack.ss_sp);
			uc->uc_stack.ss_size=0;
		}
		fcntl(socket,F_SETFL,fflag);
	}
	return bcount==0?-1:bcount;
}

int SendNet(int socket,char *buf,int n,int MTU,int TCB_no)
{
int bcount,bw;
int sz,i=0;
int fflag;
size_t SendSize;
ucontext_t *tc=NULL,*uc=get_uc(TCB_no);
unsigned long begin_stack,save_stack_size=0;
unsigned long sp;

	if(socket<0) return SYSERR;
	fflag=fcntl(socket,F_GETFL,0);
	if(uc) {
		tc=uc->uc_link;
		fcntl(socket,F_SETFL,fflag|O_NONBLOCK); //�첽����
	}
	bcount=0;
	bw=0;
	if(MTU>500) SendSize=MTU;
	else SendSize=n;
	while(bcount<n){
		sz=MIN(n-bcount,SendSize);
		if((bw=write(socket,buf,sz))>0){
			bcount+=bw;
			buf+=bw;
		}
ShowLog(5,"%s:MTU=%d,bcount=%d,n=%d,bw=%d",__FUNCTION__,MTU,bcount,n,bw);
		if(bw<0) break;
		if(bcount < n && uc) { //�л�����
		    if(!uc->uc_stack.ss_size) {
//���������ջ֡			
			swapcontext(uc,uc);
#if __WORDSIZE == 64
			begin_stack=tc->uc_mcontext.gregs[REG_RSP];
			save_stack_size=begin_stack-uc->uc_mcontext.gregs[REG_RSP];
#else
			if(uc->uc_mcontext.gregs[REG_SS] == uc->uc_link->uc_mcontext.gregs[REG_SS]) {
		  		begin_stack=tc->uc_mcontext.gregs[REG_ESP];
		  		save_stack_size=uc->uc_mcontext.gregs[REG_ESP]-begin_stack;
			}
#endif
			if(save_stack_size > MAX_STACK) save_stack_size=0; 
			if(save_stack_size==0) {	//�����Ҫ�����ջ̫֡�󣬾Ͳ�Ҫ�첽��
				uc=NULL;
				fcntl(socket,F_SETFL,fflag);
				continue;
			} 
//����fiber
			uc->uc_stack.ss_size=save_stack_size+4096;
			uc->uc_stack.ss_sp=malloc(uc->uc_stack.ss_size+16);
			if(!uc->uc_stack.ss_sp) {
				uc->uc_stack.ss_size=0;
				uc=NULL;
				fcntl(socket,F_SETFL,fflag);
				continue;
			}
//�����߳�ջ֡	
			memcpy(uc->uc_stack.ss_sp+4096,(void *)(begin_stack-save_stack_size),save_stack_size);
//��ʵ�ʵ�rsp,rbpҲ������  ����Ҫһ��asm
			sp=set_sp(uc->uc_stack.ss_sp+4096);
ShowLog(5,"%s:create fiber,save_stack_size=%08lX",__FUNCTION__,save_stack_size);
		    }
		    swapcontext(uc,uc);//��do_event֮ǰ���趨��ջ
		    if(tc != uc->uc_link) //do_event�󱻱��߳�������
			continue;
		    i=do_event(TCB_no,socket,1); //do_epoll EPOLLOUT
		    swapcontext(uc,tc); // thread escape
		}
	}
	if(uc) {
		if(uc->uc_stack.ss_sp) {// ����fiber
ShowLog(5,"%s:drop fiber",__FUNCTION__);
//�ָ��߳�ջ֡ ��Ҫһ��ASM
//���µ�uc->uc_link.uc_mcontext.gregs[REG_RSP];
#if __WORDSIZE == 64
			restore_sp((void *)uc->uc_link->uc_mcontext.gregs[REG_RSP],
				uc->uc_stack.ss_sp+uc->uc_stack.ss_size,
				uc->uc_link->uc_mcontext.gregs[REG_RBP],
				save_stack_size);
#else
			restore_sp((void *)uc->uc_link->uc_mcontext.gregs[REG_ESP],
				uc->uc_stack.ss_sp+uc->uc_stack.ss_size,
				uc->uc_link->uc_mcontext.gregs[REG_EBP],
				save_stack_size);
#endif
ShowLog(5,"%s:uc=%016lX,new=%016LX",__FUNCTION__,uc,get_uc(TCB_no));
			free(uc->uc_stack.ss_sp);
			uc->uc_stack.ss_sp=NULL;
			uc->uc_stack.ss_size=0;
		}
		fcntl(socket,F_SETFL,fflag);
	}
	return bcount==0?-1:bcount;
}

