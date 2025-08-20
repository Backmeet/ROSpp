# ROS
a port or ROS with better syntax to c++

<name> = <expression>
x = (add (10, 2)) + 32 / 3  * (54 + 2)

def <name> (arg1, arg2 ...)
return <expression>
end

def add (a, b)
return a + b
end

while (<expression>)
end
while (true)
print "yes"
end

for (<var> = <expression>; <expression>; <expression>)
end
for (i = 0; i != 10; i + 1)
print i
end

class <name>
...
public:
...
private:
...
end

class num
    public:
    self.value = 0
    def __init__ (v)
        self.value = v
    end
    def getValue ()
        return self.value
    end
    def setValue (v)
        self.value = v
    end
end

number = num (10)
print (num)                 // <object:num>
print (number)              // <object:number>
number.setValue (10)        //
print (number.getValue ())  // 10
print (number.value)        // 10