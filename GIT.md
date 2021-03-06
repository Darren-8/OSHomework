# Git

## 配置

查看本地配置

```
git config --system --list
```

查看全局配置，将会获得git用户信息。

```
git config --global --list
```

更改配置

```
git config --global user.name "用户名"
git config --global user.email "邮箱"
```

## 项目搭建

初始化项目，在当前文件夹位置创建.git文件夹及其内部文件。

```
git init
```

克隆复制一个github项目到当前文件夹位置。

```
git clone github项目的URL地址
```

## 文件基础操作

给出某次commit的具体信息

```
git show <commit>
```

查看当前文件夹中所有文件是否在暂存区。主要查看是否文件已被修改。

```
git status
```

查看指定文件是否在暂存区。

```
git status 文件名
```

添加一个文件到暂存区，跟踪此文件的修改状态。

```
git add 被跟踪文件地址
```

添加当前文件夹下的所有文件到暂存区。

```
git add .
```

删除暂存区文件，如果需要删除仓库中的文件，需要删除暂存区文件之后再提交一次。

```
git rm 文件名
```

比较工作区和暂存区文件的区别

```
git diff
```

将工作区文件恢复成暂存区文件

```
git restore 文件名
```

提交暂存区的指定文件到本地仓库，如果文件名不写，默认所有文件。

```
git commit 文件名 -m "说明信息"
```

## 文件忽略

.gitignore生效

```
git config core.excludesfile .gitignore
```

在主目录下填写.gitignore文件，内部语法为如下，被指定的文件将不会被纳入版本管理。但是对于已经进行跟踪的文件，gitignore不生效。

```
# 注释
*.txt # 忽略所有 .txt 结尾的文件。
/*.txt # 忽略当前目录下的以 .txt 结尾的文件
temp/ # 忽略 temp 文件夹下的所有文件，temp 必须是一个文件夹，不能是一个文件。
!abc.txt # 不忽略 abc.txt 文件
```

## 分支与版本控制

查看某分支的所有更改，分支名不写默认当前分支

```
git log 分支名

git long -- GIT.md
把修改了文件GIT.md的commit列出来，注意--和GIT.md之间一定要有空格

git log --author="小明"
查看作者

git log --before="2021.4.16"
git log --after="2021.4.16"
查看日期

git log --grep="Initial"
查找字符串

git log --oneline
简化输出信息

git log -S "Hello, World!"
当你想要知道 Hello, World! 字符串是什么时候加到项目中哪个文件中去的

git log --stat
显示详细信息
```

查看所有操作记录

```
git reflog
```

返回某个版本，参数为 --mixed，表示将版本树退回到某个版本，并且更改应用到暂存区，但是工作区的代码不会变化；--soft，表示将版本树退回到某个版本，但工作区和暂存区的代码都不会发生变化；--hard，表示将版本树退回到某个版本，同时工作区和暂存区的代码都发生变化。

```
git reset 参数 版本哈希值
```

新建分支，如果模板分支名不填写，则默认是当前所在分支

```
git checkout -b 分支名 模板分支名
```

删除某个分支

```
git branch -d 分支名
```

进入某个分支

```
git checkout 分支名
```

查看所有分支

```
git branch
```

合并分支，如果出现被合并的分支中改变了原分支的代码，则会产生冲突，需要手动解决冲突，增加代码不会产生冲突。

```
git merge 被合并分支名；与git fecth是经典组合
```

## 远程同步

提交到远程仓库

```
git push origin(远程主机名) 本地分支:远程分支
```

强制提交到远程仓库，覆盖原本的代码

```
git push -f
```

将远程仓库的分支和本地仓库的分支同步

```
git pull origin 远程分支:本地分支
```

如果本地新建分支，而远端没有，则需要使用如下指令，在远程仓库创建此分支的远程版本，之后再提交到远程仓库才能提交成功。

```
git push --set-upstream origin 分支名
```

查看远程仓库的分支，查看到的分支可以直接使用checkout来进入此分支。

```
git fetch
```

更新基础分支，将原分支上的修改按照时间先后应用到新分支的commit中

```
git rebase 原分支名
```

# Github

## issue

用于讨论问题

## fork

复制一个独立的仓库到自己的账户中，修改完以后可以申请Pull Request来和原本的仓库合并。