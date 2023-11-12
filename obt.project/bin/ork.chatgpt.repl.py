#!/usr/bin/env python3
#########################################
import os, time
import openai
from obt import command, path, deco

deco = deco.Deco()
#########################################

API_KEY = os.environ['OPENAI_API_KEY']
MODEL = "gpt-4-1106-preview"
#MODEL = "gpt-4"

#########################################

client = openai.OpenAI(api_key=API_KEY)
print(client)

#########################################

def upload_files(file_paths):
  file_ids = []
  ####################
  # aggregate source files
  ####################
  aggregated = ""
  for file_path in file_paths:
    path_as_str = str(file_path)
    text = open(path_as_str, "r").read()
    aggregated += text
  ####################
  # write aggregated
  ####################
  out_path = path.temp()/"orkid_aggregated.h"
  open(str(out_path), "w").write(aggregated)
  ####################
  print(deco.yellow("uploading to context:  file<%s>"%out_path))
  # TODO - upload file without chatgpt being able 
  # to see the absolute path of the file
  file = client.files.create(
    file=open(str(out_path), "rb"),
    purpose='assistants'
  )
  file_ids.append(file.id)
  return file_ids

#########################################
# Upload your files
#########################################

core_dir = path.orkid()/"ork.core"
lev2_dir = path.orkid()/"ork.lev2"
lev2_inc_dir = lev2_dir/"inc"
lev2_gfx_inc = lev2_inc_dir/"ork"/"lev2"/"gfx"
core_inc_dir = core_dir/"inc"
kern_cor_inc = core_inc_dir/"ork"/"kernel"

file_paths  = [kern_cor_inc/"orktypes.h"] 
file_paths  = [kern_cor_inc/"orkstd.h"] 
file_paths  = [kern_cor_inc/"svariant.h"] 
file_paths += [kern_cor_inc/"varmap.inl"] 
file_paths  = [kern_cor_inc/"kernel"/"treeops.inl"] 
file_paths  = [kern_cor_inc/"util"/"crc.h"] 
file_paths  = [kern_cor_inc/"util"/"scanner.h"] 
file_paths  = [kern_cor_inc/"util"/"parser.h"] 
file_paths  = [kern_cor_inc/"string"/"string.h"] 
#
file_paths += [lev2_inc_dir/"ork"/"lev2"/"lev2_types.h"] 
file_paths += [lev2_gfx_inc/"fxi.h"] 
file_paths += [lev2_gfx_inc/"gbi.h"] 
file_paths += [lev2_gfx_inc/"fbi.h"] 
file_paths += [lev2_gfx_inc/"ci.h"] 
#
file_paths += [lev2_gfx_inc/"gfxenv.h"] 
file_paths += [lev2_gfx_inc/"gfxenv_enum.h"] 
file_paths += [lev2_gfx_inc/"shadman.h"] 
file_paths += [lev2_gfx_inc/"texman.h"] 
file_paths += [lev2_gfx_inc/"rtgroup.h"] 
file_paths += [lev2_gfx_inc/"gfxmaterial.h"] 
file_paths += [lev2_gfx_inc/"shadlang.h"] 
file_paths += [lev2_gfx_inc/"shadlang_nodes.h"] 
#
file_paths += [lev2_gfx_inc/"renderer"/"renderer.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"renderable.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"rendercontext.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"renderqueue.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"renderer_enum.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"drawable.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"compositor.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"NodeCompositor"/"NodeCompositor.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"NodeCompositor"/"pbr_node_deferred.h"] 
file_paths += [lev2_gfx_inc/"renderer"/"NodeCompositor"/"pbr_node_forward.h"] 

file_ids = upload_files(file_paths)

#########################################
# Create an assistant with knowledge retrieval tool
#########################################

assistant = client.beta.assistants.create(
  instructions="You are a domain expert of a c++ game engine named Orkid. When asked questions about this engine, respond with the approprate amount of detail to the question.",
  model=MODEL,
  tools=[{"type": "retrieval"}],
  #tools=[{"type": "code_interpreter"}],
  file_ids=file_ids
)

thread = client.beta.threads.create()

#########################################
# Function to ask questions to the assistant
#########################################

def ask_question(question):
  message = client.beta.threads.messages.create(
    thread_id = thread.id,
    file_ids=file_ids,
    role="user",
    content=question
  )
  run = client.beta.threads.runs.create(
    thread_id=thread.id,
    assistant_id=assistant.id
  )
  print("waiting for reponse.")
  keep_waiting = True
  counter = 0
  while keep_waiting:
    run_status = client.beta.threads.runs.retrieve(
      thread_id = thread.id,
      run_id = run.id
    )
    if run_status.status == "completed":
      keep_waiting = False
    else:
      counter+=1
      # print spinning wheel
      if counter%4 == 0:
        print("\r|", end = '')
      elif counter%4 == 1:
        print("\r/", end = '')
      elif counter%4 == 2:
        print("\r-", end = '')
      elif counter%4 == 3:
        print("\r\\", end = '')
      time.sleep(1)
  messages = client.beta.threads.messages.list(
    thread_id=thread.id,
  )
  answer = []
  for message in messages:
    role = message.role
    content = message.content[0].text.value
    answer += [content]
  print("") 
  return answer[0]

#########################################
# repl loop
#########################################

keep_going = True
while(keep_going):
  q = input(">>> ")
  if q=="done":
    keep_going = False
  else:
    answer = ask_question(q)
    print("<<<" + deco.key(answer))

