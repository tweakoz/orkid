#!/usr/bin/env python3
#########################################
import os 
from openai import OpenAI
from obt import command, path

#########################################

API_KEY = os.environ['OPENAI_API_KEY']
#MODEL = "gpt-4-1106-preview"
MODEL = "gpt-4"

#########################################

core_dir = path.orkid()/"ork.core"
lev2_dir = path.orkid()/"ork.lev2"
lev2_inc_dir = lev2_dir/"inc"
lev2_gfx_inc = path.Path("/ork")/"lev2"/"gfx"
core_inc_dir = core_dir/"inc"
kern_cor_inc = path.Path("/ork")/"kernel"

#########################################

client = OpenAI(api_key=API_KEY)
print(client)

#########################################

def read_file_lines(file_path):
    with open(str(file_path), 'r') as file:
        for line in file:
            yield line.strip()

#########################################

def ask(question,max_tokens=0,interactive=False):
    if not interactive:
      print("<<< " + question )
    if max_tokens!=0:
      completion = client.chat.completions.create(
          model=MODEL,
          messages=[
            {
              "role": "user",
              "content": question,
            },
          ],
          max_tokens=max_tokens   # We're not expecting a meaningful response here
      )
    else:
      completion = client.chat.completions.create(
          model=MODEL,
          messages=[
            {
              "role": "user",
              "content": question,
            },
          ],
      )
    answer = completion.choices[0].message.content
    print(">>>>>> " + answer)
    return answer

#########################################

def prime_context_with_source_file(file_base,file_path):
    # 
    # Indicate that we are priming the context with a source file
    #  strip leading path (file_base) as there is no reason to prime the context with that
    # 
    summed_path = str(file_base)+str(file_path)
    #print(file_base,file_path,summed_path)

    ask(f"--- Start of file named {file_path} ---\n",max_tokens=1 )

    ###################################
    # Send each line of the file
    ###################################

    line_array = []

    def send_line_array():
        if line_array == []:
            return
        text = "\n".join(line_array)
        #print("sending: %s" % text)
        response = client.chat.completions.create(
            model=MODEL,
            messages=[
            {
                "role": "user",
                "content": text,
            }
            ],
            max_tokens=1   #Adjust as needed
        )
        #Optionally print or store the response
        answer = response.choices[0].message.content
        #print(">>>>>>> " + answer)
        line_array.clear()

    ###################################

    for line in read_file_lines(summed_path):
        print("<<: %s" % line)
        line_array.append(line)
        if len(line_array) >= 10:
            send_line_array()

    send_line_array()

    ###################################
    # Indicate the end of the file
    ###################################

    ask(f"--- End of {file_path} ---\n",max_tokens=1 )

#########################################

ask("I am cumulatively 'priming' this chatgpt client 'context' with a set ofsourcecode files 10 lines at a time (via pasting into prompt).\n" 
   +"I do not expect a response yet (until all files are primed)... The file is bounded by '--- Start of <filename>' and '--- End of <filename>' markers.\n"
   +"Please remember as much as you can about the source code, and the filenames you are about to see. Treat them collectively as source to a single c++/python project - as I will ask questions about it in a following interactive REPL.\n"
   +"In fact, when I ask 'ENUMERATE' after sending all the files, I want you to enumerate (count) all the c++ class names you detected.\n")

prime_context_with_source_file(core_inc_dir, kern_cor_inc/"svariant.h")
prime_context_with_source_file(lev2_inc_dir, lev2_gfx_inc/"targetinterfaces.h")
#prime_context_with_source_file(lev2_inc_dir, lev2_gfx_inc/"fxi.h")
#prime_context_with_source_file(lev2_inc_dir, lev2_gfx_inc/"imi.h")
#prime_context_with_source_file(lev2_inc_dir, lev2_gfx_inc/"mtxi.h")
#prime_context_with_source_file(lev2_inc_dir, lev2_gfx_inc/"fbi.h")
#prime_context_with_source_file(lev2_inc_dir, lev2_gfx_inc/"gbi.h")
#prime_context_with_source_file(lev2_inc_dir, lev2_gfx_inc/"txi.h")
#prime_context_with_source_file(lev2_inc_dir, lev2_gfx_inc/"ci.h")
#prime_context_with_source_file(lev2_inc_dir, lev2_gfx_inc/"dwi.h")

#########################################
# repl loop
#########################################

keep_going = True
while(keep_going):
  q = input(">>> ")
  if q=="done":
    keep_going = False
  else:
    ask(q,interactive=True)

#ask( 'what did I just prime this context with?')

