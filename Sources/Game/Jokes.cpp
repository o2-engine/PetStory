#include "o2/stdafx.h"
#include "Jokes.h"

#include "o2/Utils/Math/Math.h"

#include "Localization.h"

namespace Jokes
{
	// UTF-8 literals; String -> WString conversion decodes them properly for text rendering
	static const char* kJokes[][2] = {
		{ "— Доктор, меня все игнорируют! — Следующий!",
		  "— Doctor, everyone ignores me! — Next, please!" },
		{ "Объявление: «Продаю беговую дорожку. Почти новая. Причина продажи — лень».",
		  "For sale: treadmill. Barely used. Reason: laziness." },
		{ "Хотел заняться спортом. Полежал, подумал — вроде отпустило.",
		  "Wanted to take up sports. Lay down, thought it over — seems to have passed." },
		{ "— Ты чего такой довольный? — Диету закончил! — Похудел? — Нет, доел.",
		  "— Why so happy? — Finished my diet! — Lost weight? — No, finished the food." },
		{ "Записался в спортзал. Год плачу и не хожу — пусть знают, что я человек слова.",
		  "Joined a gym. A year of paying without going — so they know I'm a man of my word." },
		{ "Кот разбудил в шесть утра. Спасибо, что не в пять, как вчера.",
		  "The cat woke me at six. Thanks for not making it five, like yesterday." },
		{ "Купил самый дешёвый будильник. Теперь просыпаюсь от жадности.",
		  "Bought the cheapest alarm clock. Now greed wakes me up." },
		{ "— Что делаешь? — Ничего. — А вчера? — Не успел закончить.",
		  "— What are you doing? — Nothing. — And yesterday? — Didn't manage to finish." },
		{ "Утро добрым не бывает. Особенно если оно начинается с работы.",
		  "There's no such thing as a good morning. Especially one that starts with work." },
		{ "— Как дела? — Как в сказке: чем дальше, тем интереснее.",
		  "— How are things? — Like a fairy tale: the further in, the stranger it gets." },
		{ "Понедельник — тяжёлый день, потому что на него падает вся неделя.",
		  "Monday is a heavy day: the whole week lands on it." },
		{ "— Официант, у меня в супе муха! — Тише, а то все попросят.",
		  "— Waiter, there's a fly in my soup! — Quiet, or everyone will want one." },
		{ "Лень — двигатель прогресса: всё вокруг придумано, чтобы поменьше делать.",
		  "Laziness drives progress: everything around was invented to do less." },
		{ "— Почему опоздал? — Долго выбирал, что надеть. — И что выбрал? — Опоздать.",
		  "— Why are you late? — Couldn't pick what to wear. — And what did you pick? — Being late." },
		{ "Сначала нас учат ходить и говорить, потом просят сидеть и молчать.",
		  "First they teach us to walk and talk, then ask us to sit still and be quiet." },
		{ "Деньги — не главное. Главное — чтобы они были.",
		  "Money isn't everything. The main thing is having it." },
		{ "— Кем работаете? — Решаю проблемы. — Какие? — Которые сам создаю.",
		  "— What do you do? — I solve problems. — Which ones? — The ones I create." },
		{ "Свет быстрее звука. Поэтому некоторые кажутся умными, пока не заговорят.",
		  "Light is faster than sound. That's why some people seem smart until they speak." },
		{ "Наконец-то собрался с мыслями. Пришли не все.",
		  "Finally gathered my thoughts. Not all of them showed up." },
		{ "— Веришь в любовь с первого взгляда? — Я в очки верю.",
		  "— Do you believe in love at first sight? — I believe in glasses." },
		{ "Диета идёт отлично: аппетит уже похудел.",
		  "The diet is going great: my appetite has already lost weight." },
		{ "— Ваш главный недостаток? — Честность. — Это не недостаток! — А мне всё равно.",
		  "— Your main flaw? — Honesty. — That's not a flaw! — I don't care." },
		{ "Шахматы — мой любимый спорт: сидишь и здоровеешь.",
		  "Chess is my favorite sport: you sit there getting healthier." },
		{ "Обещал себе ничего не откладывать на завтра. Начну с понедельника.",
		  "Promised myself to stop putting things off. Starting Monday." },
		{ "— У вас есть хобби? — Да, коплю усталость.",
		  "— Any hobbies? — Yes, collecting fatigue." },
		{ "— Алло, это клуб ленивых? — Да. — Запишите меня. — Сами приходите и записывайтесь.",
		  "— Hello, is this the lazy club? — Yes. — Sign me up. — Come and do it yourself." },
		{ "Худею к лету. Уже третий год. К какому лету — не уточняю.",
		  "Getting fit for summer. Third year running. Which summer — not saying." },
		{ "— Сколько времени? — Без пятнадцати. — Без пятнадцати что? — Цифры стёрлись.",
		  "— What time is it? — Quarter to. — Quarter to what? — The digits wore off." },
		{ "Внутренний голос сказал: «Иди тренируйся». Потом добавил: «Шучу, поешь».",
		  "My inner voice said: 'Go work out'. Then added: 'Kidding, go eat'." },
		{ "Опыт — это то, что появляется сразу после того, как было нужно.",
		  "Experience is what shows up right after you needed it." },
		{ "— Рано встаёте? — В шесть утра. — И что делаете? — Сплю дальше, но с гордостью.",
		  "— Early riser? — Six a.m. — Then what? — Back to sleep, but with pride." },
		{ "Уборка — это перекладывание вещей в более интересные места.",
		  "Cleaning is moving things to more interesting places." },
		{ "Абонемент в зал окупился: похудел кошелёк.",
		  "The gym membership paid off: my wallet lost weight." },
		{ "— Как вам новая работа? — Работа мечты: сижу и мечтаю.",
		  "— How's the new job? — A dream job: I sit and dream." },
		{ "Если долго смотреть на будильник, можно опоздать куда угодно.",
		  "Stare at the alarm clock long enough and you can be late for anything." },
		{ "«Мне нечего надеть», — подумал шкаф и не закрылся.",
		  "'I have nothing to wear,' thought the wardrobe, and wouldn't close." },
		{ "Новая жизнь с понедельника отменяется: понедельник не пришёл на встречу.",
		  "New life postponed: Monday didn't show up to the meeting." },
		{ "— Почему молчишь? — Обдумываю ответ на вопрос, который ты ещё не задала.",
		  "— Why so quiet? — Thinking over my answer to the question you haven't asked yet." },
		{ "Пунктуальность — умение угадать, на сколько опоздают остальные.",
		  "Punctuality is the art of guessing how late everyone else will be." },
		{ "Забыл, зачем зашёл в комнату. Вышел. Вспомнил. Зашёл. Забыл.",
		  "Forgot why I entered the room. Left. Remembered. Came back. Forgot." },
		{ "— Как проходит день? — По плану. — По какому? — По чужому.",
		  "— How's your day? — Going to plan. — Whose plan? — Someone else's." },
		{ "Хорошо там, где нас нет. Видимо, дело всё-таки в нас.",
		  "The grass is greener where we aren't. So maybe it's us after all." },
		{ "Терпение и труд всё перетрут. Особенно терпение, пока труд отдыхает.",
		  "Patience and hard work conquer all. Especially patience, while hard work rests." },
		{ "— Опишите себя тремя словами. — Ленивый.",
		  "— Describe yourself in three words. — Lazy." },
		{ "Утренняя зарядка у меня есть: заряжаю телефон.",
		  "I do have a morning charge routine: I charge my phone." },
		{ "Навигатор сказал: «Вы прибыли». Приятно, что хоть кто-то в меня верит.",
		  "The GPS said: 'You have arrived'. Nice to know someone believes in me." },
		{ "— Ты жаворонок или сова? — Я ленивец.",
		  "— Early bird or night owl? — I'm a sloth." },
		{ "Всё, что нас не убивает, делает нас осторожнее.",
		  "Whatever doesn't kill us makes us more careful." },
		{ "Мозг: «Вставай, проспишь!» Он же через минуту: «Ладно, ещё пять минуточек».",
		  "Brain: 'Get up, you'll oversleep!' Same brain a minute later: 'Fine, five more minutes.'" },
		{ "Идеальный порядок в доме — когда знаешь, в какой куче что лежит.",
		  "A perfectly tidy home is knowing which pile holds what." },
	};

	static const int kCount = sizeof(kJokes) / sizeof(kJokes[0]);

	int Count()
	{
		return kCount;
	}

	String At(int index)
	{
		auto& joke = kJokes[Math::Clamp(index, 0, kCount - 1)];
		return Loc::Tr(joke[0], joke[1]);
	}

	String Random()
	{
		// Random(0, kCount) truncates uniformly to [0, kCount-1]; the top edge case is clamped by At
		return At(Math::Random(0, kCount));
	}
}
