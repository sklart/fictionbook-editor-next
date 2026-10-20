# Документ перенесён: sequence-property-schema-plan

Актуальный вход: [reference/sequence-property-schema.md](reference/sequence-property-schema.md).

Полный прежний текст: [историческая редакция](archive/shell/sequence-property-schema-plan.md).

Этот файл сохраняет совместимость ранее опубликованного пути. Новые ссылки следует вести на тематический раздел.

## Прежние якоря

<a id="план-отдельной-property-schema-для-sequence"></a>
[Перейти к соответствующему разделу](archive/shell/sequence-property-schema-plan.md#%D0%BF%D0%BB%D0%B0%D0%BD-%D0%BE%D1%82%D0%B4%D0%B5%D0%BB%D1%8C%D0%BD%D0%BE%D0%B9-property-schema-%D0%B4%D0%BB%D1%8F-sequence).

<a id="почему-sequence-нельзя-маппить-как-нибудь"></a>
[Перейти к соответствующему разделу](archive/shell/sequence-property-schema-plan.md#%D0%BF%D0%BE%D1%87%D0%B5%D0%BC%D1%83-sequence-%D0%BD%D0%B5%D0%BB%D1%8C%D0%B7%D1%8F-%D0%BC%D0%B0%D0%BF%D0%BF%D0%B8%D1%82%D1%8C-%D0%BA%D0%B0%D0%BA-%D0%BD%D0%B8%D0%B1%D1%83%D0%B4%D1%8C).

<a id="практический-план"></a>
[Перейти к соответствующему разделу](archive/shell/sequence-property-schema-plan.md#%D0%BF%D1%80%D0%B0%D0%BA%D1%82%D0%B8%D1%87%D0%B5%D1%81%D0%BA%D0%B8%D0%B9-%D0%BF%D0%BB%D0%B0%D0%BD).

<a id="следующий-конкретный-шаг"></a>
[Перейти к соответствующему разделу](archive/shell/sequence-property-schema-plan.md#%D1%81%D0%BB%D0%B5%D0%B4%D1%83%D1%8E%D1%89%D0%B8%D0%B9-%D0%BA%D0%BE%D0%BD%D0%BA%D1%80%D0%B5%D1%82%D0%BD%D1%8B%D0%B9-%D1%88%D0%B0%D0%B3).

<a id="целевое-состояние"></a>
[Перейти к соответствующему разделу](archive/shell/sequence-property-schema-plan.md#%D1%86%D0%B5%D0%BB%D0%B5%D0%B2%D0%BE%D0%B5-%D1%81%D0%BE%D1%81%D1%82%D0%BE%D1%8F%D0%BD%D0%B8%D0%B5).

<a id="что-пока-сознательно-не-делаем"></a>
[Перейти к соответствующему разделу](archive/shell/sequence-property-schema-plan.md#%D1%87%D1%82%D0%BE-%D0%BF%D0%BE%D0%BA%D0%B0-%D1%81%D0%BE%D0%B7%D0%BD%D0%B0%D1%82%D0%B5%D0%BB%D1%8C%D0%BD%D0%BE-%D0%BD%D0%B5-%D0%B4%D0%B5%D0%BB%D0%B0%D0%B5%D0%BC).

<a id="что-уже-есть-в-проекте"></a>
[Перейти к соответствующему разделу](archive/shell/sequence-property-schema-plan.md#%D1%87%D1%82%D0%BE-%D1%83%D0%B6%D0%B5-%D0%B5%D1%81%D1%82%D1%8C-%D0%B2-%D0%BF%D1%80%D0%BE%D0%B5%D0%BA%D1%82%D0%B5).

<a id="этап-1-выделить-идентификаторы"></a>
[Перейти к соответствующему разделу](archive/shell/sequence-property-schema-plan.md#%D1%8D%D1%82%D0%B0%D0%BF-1-%D0%B2%D1%8B%D0%B4%D0%B5%D0%BB%D0%B8%D1%82%D1%8C-%D0%B8%D0%B4%D0%B5%D0%BD%D1%82%D0%B8%D1%84%D0%B8%D0%BA%D0%B0%D1%82%D0%BE%D1%80%D1%8B).

<a id="этап-2-подготовить-property-schema"></a>
[Перейти к соответствующему разделу](archive/shell/sequence-property-schema-plan.md#%D1%8D%D1%82%D0%B0%D0%BF-2-%D0%BF%D0%BE%D0%B4%D0%B3%D0%BE%D1%82%D0%BE%D0%B2%D0%B8%D1%82%D1%8C-property-schema).

<a id="этап-3-подготовить-операционные-скрипты"></a>
[Перейти к соответствующему разделу](archive/shell/sequence-property-schema-plan.md#%D1%8D%D1%82%D0%B0%D0%BF-3-%D0%BF%D0%BE%D0%B4%D0%B3%D0%BE%D1%82%D0%BE%D0%B2%D0%B8%D1%82%D1%8C-%D0%BE%D0%BF%D0%B5%D1%80%D0%B0%D1%86%D0%B8%D0%BE%D0%BD%D0%BD%D1%8B%D0%B5-%D1%81%D0%BA%D1%80%D0%B8%D0%BF%D1%82%D1%8B).

<a id="этап-4-подключить-sequence-к-modern-property-handler"></a>
[Перейти к соответствующему разделу](archive/shell/sequence-property-schema-plan.md#%D1%8D%D1%82%D0%B0%D0%BF-4-%D0%BF%D0%BE%D0%B4%D0%BA%D0%BB%D1%8E%D1%87%D0%B8%D1%82%D1%8C-sequence-%D0%BA-modern-property-handler).

<a id="этап-5-ручная-gui-проверка"></a>
[Перейти к соответствующему разделу](archive/shell/sequence-property-schema-plan.md#%D1%8D%D1%82%D0%B0%D0%BF-5-%D1%80%D1%83%D1%87%D0%BD%D0%B0%D1%8F-gui-%D0%BF%D1%80%D0%BE%D0%B2%D0%B5%D1%80%D0%BA%D0%B0).
