string domain = project.Variables["domain"].Value;


//проверка переменных на пустоту
string acc = project.Variables["acc"].Value;
string[] parts = acc.Split('|');
string login = "";
string password = "";
string proxyDetails = project.Variables["proxy"].Value;

if (parts.Length == 2)
{
    login = parts[0];
    password = parts[1];
}
else if (parts.Length == 3)
{
    login = parts[0];
    password = parts[1];
    proxyDetails = parts[2];
    project.Variables["proxy"].Value = proxyDetails;
}
else
{
    project.SendToLog("Ошибка: переменная acc не содержит нужное количество данных, разделённых символом '|'.", ZLog.Error, true, ZColor.Red);
    throw new Exception("Некорректный формат данных аккаунта.");
}

//обозначаем перемменные для настройки smtp и imap
string imapServer = "";
string smtpServer = "";
int imapPort = 993;
int smtpPort = 587;


string service = project.Variables["service"].Value;
if (string.IsNullOrEmpty(service))
{
	project.SendToLog("Не выбран почтовый сервис для аккаунтов", ZLog.Error, true, ZColor.Red);
	throw new Exception("Не указан почтовый сервис.");
}

switch (service)
{
	case "Mail.ru":
		imapServer = "imap.mail.ru";
		smtpServer = "smtp.mail.ru";
		break;
	case "Yandex":
		imapServer = "imap.yandex.ru";
		smtpServer = "smtp.yandex.ru";
		smtpPort = 465;
		break;
	case "Google":
		imapServer = "imap.gmail.com";
		smtpServer = "smtp.gmail.com";
		break;
	default:
		project.SendToLog("Указан неизвестный почтовый сервис", ZLog.Error, true, ZColor.Red);
		throw new Exception("Неизвестный почтовый сервис.");
}

// Инициализация переменных на основе флагов из ZennoPoster
int markAsReadProbability = int.Parse(project.Variables["markAsReadProbability"].Value);
int replyToMessageProbability = int.Parse(project.Variables["replyToMessageProbability"].Value);
int forwardMessageProbability = int.Parse(project.Variables["forwardMessageProbability"].Value);
int deleteUnreadProbability = int.Parse(project.Variables["deleteUnreadProbability"].Value);
int deleteReadProbability = int.Parse(project.Variables["deleteReadProbability"].Value);
int markImportantProbability = int.Parse(project.Variables["markImportantProbability"].Value);
int archiveProbability = int.Parse(project.Variables["archiveProbability"].Value);
int addContactsProbability = int.Parse(project.Variables["addContactsProbability"].Value);
int extractFromSpamProbability = int.Parse(project.Variables["extractFromSpamProbability"].Value);
int clickLinkProbability = int.Parse(project.Variables["clickLinkProbability"].Value);


// Случайный генератор чисел для вероятностных действий
Random random = new Random();


void ReadUnreadMailsFromSenderDomain(ImapClient client, string folderName, string senderDomain)
{
	IMailFolder folder = null;
	try
	{
		folder = client.GetFolder(folderName);
		folder.Open(FolderAccess.ReadWrite);

		var query = SearchQuery.NotSeen;
		if (!string.IsNullOrWhiteSpace(senderDomain))
		{
			query = query.And(SearchQuery.HeaderContains("From", senderDomain));
		}

		//пробегаемся по каждому сообщению
		foreach (var uid in folder.Search(query))
		{
			var message = folder.GetMessage(uid);
			project.SendToLog($"Нашли непрочитанное сообщение от {message.From}", ZLog.Info, true, ZColor.Gray);
			// Пауза перед действиями
			System.Threading.Thread.Sleep(1000 + new Random().Next(1000));


			// Удаление непрочитанного письма по вероятности
			if (random.Next(100) < deleteUnreadProbability)
			{
				folder.AddFlags(uid, MessageFlags.Deleted, true);
				folder.Expunge(); // Применить удаление сразу
				project.SendToLog($"Непрочитанное сообщение от {message.From}: {message.Subject} удалено", ZLog.Info, true, ZColor.Turquoise);
				continue ; // Пропустить обработку этого письма
			}


			// Помечаем сообщение как прочитанное
			bool messageMarkedAsRead = false;
			if (random.Next(100) < markAsReadProbability)
			{
				folder.AddFlags(uid, MessageFlags.Seen, true);
				project.SendToLog($"Сообщение от {message.From}: {message.Subject} помечено как прочитанное", ZLog.Info, true, ZColor.Turquoise);
				messageMarkedAsRead = true;
			}
			System.Threading.Thread.Sleep(1000 + new Random().Next(1000));


			// Ответ на письмо
			if (random.Next(100) < replyToMessageProbability)
			{
				//рандомизация ответа из строки текста и строки смайла
				int rnd_ans = new Random().Next(1, 3);
				IZennoList list = project.Lists["Ответы"];
				string ans = list.ElementAt(new Random().Next(0, list.Count));

				IZennoList list2 = project.Lists["Смайлы"];
				string sml = list2.ElementAt(new Random().Next(0, list2.Count));

				string Spaces = string.Concat(Enumerable.Repeat(" ", new Random().Next(1, 5)));
				string Spaces2 = string.Concat(Enumerable.Repeat(" ", new Random().Next(1, 3)));
				string answer_text = "";

				if (rnd_ans == 1)
				{
					answer_text = ans + Spaces + sml + Spaces2;
				}
				else if (rnd_ans == 2)
				{
					answer_text = sml + Spaces + ans + Spaces2;
				}
				string to = $"{message.From}";

				SendEmail(login, password, to, "Re: " + message.Subject, answer_text);
				project.SendToLog($"Ответ отправлен на сообщение от {message.From}: {message.Subject}", ZLog.Info, true, ZColor.Turquoise);
			}
			System.Threading.Thread.Sleep(1000 + new Random().Next(1000));


			// Регулярное выражение для поиска email-адресов
			string emailPattern = @"\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Z|a-z]{2,}\b";

			// Чтение строк из файла и извлечение email-адресов
			var lines = File.ReadAllLines(project.Directory + @"\Аккаунты.txt");
			var emailList = new List<string>();

			foreach (var line in lines)
			{
				var matches = Regex.Matches(line, emailPattern);
				foreach (Match match in matches)
				{
					if (!match.Value.Equals(login, StringComparison.OrdinalIgnoreCase))
					{
						emailList.Add(match.Value);
					}
				}
			}

			// Пересылка письма
			if (random.Next(100) < forwardMessageProbability && emailList.Count > 0)
			{
				var forwardToEmail = emailList[random.Next(emailList.Count)];
				SendEmail(login, password, forwardToEmail, "Fwd: " + message.Subject, "Пересылаю интересное сообщение: \n\n" + message.TextBody);
				project.SendToLog($"Сообщение переслано на {forwardToEmail}", ZLog.Info, true, ZColor.Turquoise);
			}
			System.Threading.Thread.Sleep(1000 + new Random().Next(1000));



			// Удаление прочитанного письма
			if (messageMarkedAsRead && random.Next(100) < deleteReadProbability)
			{
				folder.AddFlags(uid, MessageFlags.Deleted, true);
				folder.Expunge();
				project.SendToLog($"Прочитанное сообщение от {message.From}: {message.Subject} удалено", ZLog.Info, true, ZColor.Turquoise);
				continue ;
			}
			System.Threading.Thread.Sleep(1000 + new Random().Next(1000));


			// Отметка письма важным
			if (random.Next(100) < markImportantProbability)
			{
				folder.AddFlags(uid, MessageFlags.Flagged, true); // Флаг важности
				project.SendToLog($"Сообщение от {message.From}: {message.Subject} отмечено как важное", ZLog.Info, true, ZColor.LightBlue);
			}
			System.Threading.Thread.Sleep(1000 + new Random().Next(1000));

			if (service == "Yandex")
			{
				// Перемещение письма в архив
				if (random.Next(1000) < archiveProbability)
				{
					var archiveFolder = client.GetFolder("Archive");
					folder.MoveTo(uid, archiveFolder);
					project.SendToLog($"Сообщение от {message.From}: {message.Subject} перемещено в архив", ZLog.Info, true, ZColor.LightBlue);
				}
				System.Threading.Thread.Sleep(2000 + new Random().Next(1000));
			}

			// Добавление в контакты отправителя
			if (random.Next(100) < addContactsProbability)
			{
				// Добавление в контакты — здесь пример вызова API вашего почтового клиента или другой системы
				project.SendToLog($"Отправитель {message.From} добавлен в контакты", ZLog.Info, true, ZColor.LightBlue);
			}
			System.Threading.Thread.Sleep(1000 + new Random().Next(1000));


			// Найти и кликнуть ссылку в письме (кроме последней) по вероятности
			if (random.Next(100) < clickLinkProbability && message.HtmlBody != null)
			{
				var links = Regex.Matches(message.HtmlBody, @"href=[""'](http[^""']+)[""']");
				if (links.Count > 1) // Исключаем последнюю ссылку (предположительно, это отписка)
				{
					var linkToClick = links[links.Count - 2].Groups[1].Value; // Предпоследняя ссылка
					// Логика для "клика" по ссылке, например, HTTP GET запрос
					project.SendToLog($"Клик по ссылке в сообщении: {linkToClick}", ZLog.Info, true, ZColor.LightBlue);
				}
			}

			// Пауза между обработкой писем
			System.Threading.Thread.Sleep(2000 + new Random().Next(1000));

			// --------------------------END------------------------------//
		}

	}
	catch (Exception ex)
	{
		project.SendToLog("Ошибка при чтении писем: " + ex.Message, ZLog.Error, true, ZColor.Red);
		throw new Exception();
	}
	finally
	{
		folder? .Close();
	}
};


// Проверка спама и перемещение писем из спама
void CheckAndMoveFromSpam(ImapClient client, string spamFolderName, string inboxFolderName, string senderDomain)
{
	try
	{
		var spamFolder = client.GetFolder(spamFolderName);
		spamFolder.Open(FolderAccess.ReadWrite);
		var inboxFolder = client.GetFolder(inboxFolderName);

		var query = SearchQuery.FromContains(senderDomain);
		foreach (var uid in spamFolder.Search(query))
		{
			if (random.Next(100) < extractFromSpamProbability)
			{
				spamFolder.MoveTo(uid, inboxFolder);
				project.SendToLog($"Сообщение от {senderDomain} перемещено из спама во входящие", ZLog.Info, true, ZColor.LightBlue);
			}
		}
		spamFolder.Close();
	}
	catch (Exception ex)
	{
		project.SendToLog("Ошибка при обработке спама: " + ex.Message, ZLog.Error, true, ZColor.Red);
	}
};


// функция отправки сообщения по smtp
void SendEmail(string fromEmail, string fromPassword, string toEmail, string subject, string body)
{
	try
	{
		var smtpClient = new SmtpClient(smtpServer)
		{
			Port = smtpPort,
			Credentials = new NetworkCredential(fromEmail, fromPassword),
			EnableSsl = true,
		};

		// Создание сообщения
		var mailMessage = new MailMessage
		{
			From = new MailAddress(fromEmail),
			Subject = subject,
			Body = body,
			IsBodyHtml = true,
		};
		mailMessage.To.Add(toEmail);
		//Отправка
		smtpClient.Send(mailMessage);
	}
	catch (Exception ex)
	{
		project.SendToLog("Ошибка при отправке email: " + ex.Message, ZLog.Error, true, ZColor.Red);
	}
};

// Подключение и авторизация:
using (var client = new ImapClient())
{
	try
	{
		bool useProxy = project.Variables["use_proxy"].Value.Equals("True", StringComparison.OrdinalIgnoreCase);
		if (useProxy)
		{
			if (string.IsNullOrEmpty(proxyDetails))
			{
				throw new Exception("Данные прокси не указаны.");
			}
			var proxyUri = new Uri(proxyDetails);
			string proxyHost = proxyUri.Host;
			int proxyPort = proxyUri.Port;
			string proxyUser = proxyUri.UserInfo.Split(':')[0];
			string proxyPassword = proxyUri.UserInfo.Split(':')[1];

			NetworkCredential myCredentials = new NetworkCredential(proxyUser, proxyPassword);
			client.ProxyClient = new MailKit.Net.Proxy.Socks5Client(proxyHost, proxyPort, myCredentials);
		}

		client.Connect(imapServer, imapPort, MailKit.Security.SecureSocketOptions.Auto);
		client.Authenticate(login, password);
		
		//логирование всех папок
		foreach (var folder in client.GetFolders(client.PersonalNamespaces[0]))
		{
			project.SendToLog(folder.Name, ZLog.Info, true, ZColor.Yellow);
		}

		// Обработка спама в зависимости от сервиса
        if (service == "Mail.ru")
        {
            CheckAndMoveFromSpam(client, "Спам", "INBOX", domain);
        }
        else if (service == "Google")
        {
            string spamFolderName = "[Gmail]/Спам"; // Используйте точное имя папки спама
            CheckAndMoveFromSpam(client, spamFolderName, "INBOX", domain);
        }
        else if (service == "Yandex")
        {
            CheckAndMoveFromSpam(client, "Спам", "INBOX", domain);
        }
		
		ReadUnreadMailsFromSenderDomain(client, "INBOX", domain);
		project.SendToLog("Успешно обработы сообщения и отключение от IMAP сервера", ZLog.Info, true, ZColor.Green);
	}
	catch (Exception ex)
	{
		project.SendToLog("Ошибка подключения или аутентификации: " + ex.Message, ZLog.Error, true, ZColor.Red);
		throw new Exception();
	}
	finally
	{
		if (client.IsConnected)
		{
			client.Disconnect(true);
		}
	}
}
