print("\
\
")
 
headers["Content-Type"] = "text/html; charset=utf-8"

local oldtostring = tostring
function tostring(o, fmt)
  local mt = getmetatable(o)
  if type(mt) == "table" and mt.__tostring then
    return mt.__tostring(o, fmt)
  else
    return oldtostring(o)
  end
end

page_title = "Hello World"
items = {
  ["Apples"] = "Fruit",
  ["Evil kitten"] = "Animal"
}


print("
<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">
<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">
  <head>
    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\"/>
    <title>")
print( page_title )
print("</title>
  </head>
  ")
--[[ This is a comment ]]
print("
  <body>
    <ul>
    ")
for k, v in pairs(header) do 
print("
      <li><b>")
print( k )
print("</b> = ")
print(v)
print("</li>
    ")
end 
print("
    </ul>
  </body>  
  ")
--[[ 
    Comments
    can be
    multiline
  ]]
print("
</html>
")
