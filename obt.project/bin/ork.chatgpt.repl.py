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
lev2_dir = path.orkid()/"ork.lev2"
lev2_inc_dir = lev2_dir/"inc"/"ork"/"lev2"
#########################################
client = OpenAI(api_key=API_KEY)
print(client)
#########################################
def read_file_lines(file_path):
    with open(str(file_path), 'r') as file:
        for line in file:
            yield line.strip()
#########################################
def ask(question):
    print("<<< " + question )
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
    print(file_base,file_path,summed_path)

    ask("I am cumulatively 'priming' this chatgpt client 'context' with the following sourcecode file: %s a 10 lines at a time" % summed_path )
    ask("I do not expect a response yet (until all files are loaded)... The file is bounded by --- Start of and --- End of markers"  )
    start_marker = f"--- Start of {file_path} ---\n"
    print(start_marker)
    client.chat.completions.create(
        model=MODEL,
        messages=[
          {
            "role": "user",
            "content": start_marker,
          }
        ],
        max_tokens=1   # We're not expecting a meaningful response here
    )

    ###################################
    # Send each line of the file
    ###################################

    line_array = []

    def send_line_array():
        if line_array == []:
            return
        text = "\n".join(line_array)
        print("sending: %s" % text)
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
        print(">>>>>>> " + answer)
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

    end_marker = f"--- End of {file_path} ---\n"
    client.chat.completions.create(
        model=MODEL,
        messages=[
        {
            "role": "user",
            "content": end_marker,
        }
        ],
        max_tokens=1   #We're not expecting a meaningful response here
    )

#########################################

#prime_context_with_source_file(lev2_inc_dir, path.Path("/")/"gfx"/"targetinterfaces.h")

ask("did you receive the file ?")

#ask( 'what did I just prime this context with?')

