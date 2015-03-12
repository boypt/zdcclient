## 项目目标 ##
作为开源的第三方的supplicant客户端兼容神州数码的校园网认证系统，支持在Unix系操作系统下跨平台使用。

本项目已经停止多年，无法保证程序可用，源码在仓库中，仅供研究，欢迎Fork。

目前有以下Fork：

  * https://github.com/isombyt/zdcclient

## 特性 ##
  * 基于pcap的驱动级数据包包过滤，极低的CPU占用；
  * 纯C语言编写，极低的内存占用；
  * 尽可能少的库依赖，容易编译；

## 文档 ##
  * UserManual 用户手册
  * DeveloperDocument 开发者手册，获取、编译源代码

## 远期理想 ##
开发兼容所有国内常见的校园网络认证系统的程序接口，统一gui界面的认证程序。

## 闲话 ##
话说，某数码公司的软件做得很烂很烂，即使在Windows下面经常自动退出、出错，用户烦不胜烦，甚至，其软件在Vista、Win 7等系统都没能很好地支持，死往Program File写文件，更不用说不支持Windows以外的操作系统了……

## ZDClient ##
得益于很多技术爱好者的自发研究，通过分析协议报文，基本掌握了supplicant的协议，ZDClient就是通过分析研究部署在作者所在学校的“神州数码”，以及参考前人的结果（姚琦的Java版），使用C语言开发跨平台的认证客户端。

## DCBA协议 ##
神州数码的官方客户端同时带有两种认证协议：802.1x协议和所谓的**DCBA认证协议**，ZDClient只支持前者，如果需要在DCBA协议版本的客户端，请关注[aecium](http://gitorious.org/aecium)项目，[aecium](http://gitorious.org/aecium)是为“安腾”的BAS系统开发的客户端，但经证实与神州数码的DCBA协议兼容。

## 最新动态 ##
  * MacOS 系统图形界面版： [下载](http://garning.com/forever/garning_13.html) by Insion
  * 分支出一个兼容神州数码客户端的"标准802.1x"模式认证的版本，适合人大的校园网
  * 0.12版代码支持在MacOS/BSD系列系统内编译运行，[r94](https://code.google.com/p/zdcclient/source/detail?r=94)
  * 基于0.11的代码开发的win32版本；
  * [错误更正]：r62版本的1.1下载包中的install脚本出错，没法完成安装，请更新r63版的下载包；
  * ZDClient 0.11 增加可通过notify显示服务器信息的启动脚本；增加安装脚本；默认版本改成3.5.04.1110fk；山东聊城大学测试可用
  * ZDClient 0.10 增加显示服务器发回的中文提示信息。
  * ZDClient 0.9 二进制包采用pcap的静态编译，应该在所有系统中都可以直接运行，欢迎大家测试。
  * ZDClient 0.8 改变副本检测、后台运行的方式(避免出现问题进程)
  * ZDClient 0.7 加入程序独立检查和退出选项-l
  * ZDClient 0.6 修正些少影响不大的协议处理
  * ZDClient 0.5 完善了DHCP过程的IP获取策略。
  * ZDClient 0.4 经过武汉大学同学的测试，**据说**perfect地完成了认证上网！

广州大学发来贺电！欢迎其他网友积极测试……（广州大学的校园网认证系统已经换成锐捷了，我没有测试环境，所以可能这个项目不会有太多更新了。）


---

### 联系作者 ###
  * ![http://zruijie4gzhu.googlecode.com/files/mail.png](http://zruijie4gzhu.googlecode.com/files/mail.png) **Mail and Gtalk**
  * [Twitter @BOYPT](http://twitter.com/BOYPT)
  * [Blog](http://apt-blog.net/)
**本项目完全是网友自发启动，和“神州数码网络有限公司”没有任何关系。**