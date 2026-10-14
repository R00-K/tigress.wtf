#include <stdio.h>
#include <string.h>

char states[20][3];
char transitions[50][3][3];
char result[20][3];
int transition_count = 0;

int is_in_result(char state[3],int count){
int i;
for(i=0;i<count;i++){
if(strcmp(result[i],state)==0)
return 1;
}
 return 0;
 }
 
   int add_state(char state[3],int count){
   if (!is_in_result(state,count)){
   strcpy(result[count],state);
   count++;
   }
   return count;
  }
 
   void compute_epsilon_closure(char current_states[3]){
   int i,j;
   int count = 0 ;
   
   strcpy(result[count],current_states);
   count++;
   
   for(i=0;i<count;i++) {
   char state_to_check[3];
   strcpy(state_to_check,result[i]);
   
   for(j = 0;j < transition_count; j++){
   if (strcmp(transitions[j][0],state_to_check)== 0 &&
   strcmp(transitions[j][1],"e")==0){
   
   count = add_state(transitions[j][2],count);
   }
  }
 }
 
 printf("epsilon closure of %s = {",current_states);
 for (i=0;i<count;i++) {
 printf("%s",result[i]);
 }
 printf("}\n");
}
 
  int main(){
  FILE *fp;
  int n,i;
 
  fp=fopen("input.txt","r");
  if (!fp){
  printf("cannot open input.txt\n");
  return 1;
 }
 
 printf("enter the number of states:\n");
 scanf("%d",&n);
  printf("enter the states:\n");
 for(i = 0;i<n;i++){
 scanf("%s",states[i]);
 }
 
 while (fscanf(fp,"%s %s %s ",
      transitions[transition_count][0],
      transitions[transition_count][1],
      transitions[transition_count][2]) !=EOF){
    transition_count++;
    }
   
    fclose(fp);
   
    for(i = 0;i<n;i++){
      compute_epsilon_closure(states[i]);
      }
     
      return 0;
 }


